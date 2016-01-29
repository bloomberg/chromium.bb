// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.support.v4.widget.DrawerLayout;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ViewSwitcher;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.Stack;

/**
 * The new bookmark manager that is planned to replace the existing bookmark manager. It holds all
 * views and shared logics between tablet and phone. For tablet/phone specific logics, see
 * {@link EnhancedBookmarkActivity} (phone) and {@link EnhancedBookmarkPage} (tablet).
 */
public class EnhancedBookmarkManager implements EnhancedBookmarkDelegate {
    private static final int FAVICON_MAX_CACHE_SIZE_BYTES = 10 * 1024 * 1024; // 10MB

    private Activity mActivity;
    private ViewGroup mMainView;
    private EnhancedBookmarksModel mEnhancedBookmarksModel;
    private EnhancedBookmarkUndoController mUndoController;
    private final ObserverList<EnhancedBookmarkUIObserver> mUIObservers =
            new ObserverList<EnhancedBookmarkUIObserver>();
    private Set<BookmarkId> mSelectedBookmarks = new HashSet<>();
    private EnhancedBookmarkStateChangeListener mUrlChangeListener;
    private EnhancedBookmarkContentView mContentView;
    private EnhancedBookmarkSearchView mSearchView;
    private ViewSwitcher mViewSwitcher;
    private DrawerLayout mDrawer;
    private EnhancedBookmarkDrawerListView mDrawerListView;
    private final Stack<EnhancedBookmarkUIState> mStateStack = new Stack<>();
    private LargeIconBridge mLargeIconBridge;
    private String mInitialUrl;
    private boolean mIsDialogUi;

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            // If the folder is removed in folder mode, show the parent folder or falls back to all
            // bookmarks mode.
            if (getCurrentState() == EnhancedBookmarkUIState.STATE_FOLDER
                    && node.getId().equals(mStateStack.peek().mFolder)) {
                if (mEnhancedBookmarksModel.getTopLevelFolderIDs(true, true).contains(
                        node.getId())) {
                    openAllBookmarks();
                } else {
                    openFolder(parent.getId());
                }
            }
            clearSelection();
        }

        @Override
        public void bookmarkNodeMoved(BookmarkItem oldParent, int oldIndex, BookmarkItem newParent,
                int newIndex) {
            clearSelection();
        }

        @Override
        public void bookmarkModelChanged() {
            // If the folder no longer exists in folder mode, we need to fall back. Relying on the
            // default behavior by setting the folder mode again.
            if (getCurrentState() == EnhancedBookmarkUIState.STATE_FOLDER) {
                setState(mStateStack.peek());
            }
            clearSelection();
        }
    };

    private final Runnable mModelLoadedRunnable = new Runnable() {
        @Override
        public void run() {
            mSearchView.onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkManager.this);
            mDrawerListView.onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkManager.this);
            mContentView.onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkManager.this);
            if (!TextUtils.isEmpty(mInitialUrl)) {
                setState(EnhancedBookmarkUIState.createStateFromUrl(mInitialUrl,
                        mEnhancedBookmarksModel));
            }
        }
    };

    /**
     * Creates an instance of {@link EnhancedBookmarkManager}. It also initializes resources,
     * bookmark models and jni bridges.
     * @param activity The activity context to use.
     * @param isDialogUi Whether the main bookmarks UI will be shown in a dialog, not a NativePage.
     */
    public EnhancedBookmarkManager(Activity activity, boolean isDialogUi) {
        mActivity = activity;
        mIsDialogUi = isDialogUi;

        mEnhancedBookmarksModel = new EnhancedBookmarksModel();
        mMainView = (ViewGroup) mActivity.getLayoutInflater().inflate(R.layout.eb_main, null);
        mDrawer = (DrawerLayout) mMainView.findViewById(R.id.eb_drawer_layout);
        mDrawerListView = (EnhancedBookmarkDrawerListView) mMainView.findViewById(
                R.id.eb_drawer_list);
        mContentView = (EnhancedBookmarkContentView) mMainView.findViewById(R.id.eb_content_view);
        mViewSwitcher = (ViewSwitcher) mMainView.findViewById(R.id.eb_view_switcher);
        mUndoController = new EnhancedBookmarkUndoController(activity, mEnhancedBookmarksModel,
                ((SnackbarManageable) activity).getSnackbarManager());
        mSearchView = (EnhancedBookmarkSearchView) getView().findViewById(R.id.eb_search_view);
        mEnhancedBookmarksModel.addObserver(mBookmarkModelObserver);
        initializeToLoadingState();
        mEnhancedBookmarksModel.runAfterBookmarkModelLoaded(mModelLoadedRunnable);

        // Load partner bookmarks explicitly. We load partner bookmarks in the deferred startup
        // code, but that might be executed much later. Especially on L, showing loading
        // progress bar blocks that so it won't be loaded. http://crbug.com/429383
        PartnerBookmarksShim.kickOffReading(activity);

        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ApplicationStatus
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min(activityManager.getMemoryClass() / 4 * 1024 * 1024,
                FAVICON_MAX_CACHE_SIZE_BYTES);
        mLargeIconBridge.createCache(maxSize);
    }

    /**
     * Destroys and cleans up itself. This must be called after done using this class.
     */
    public void destroy() {
        for (EnhancedBookmarkUIObserver observer : mUIObservers) {
            observer.onDestroy();
        }
        assert mUIObservers.size() == 0;

        if (mUndoController != null) {
            mUndoController.destroy();
            mUndoController = null;
        }
        mEnhancedBookmarksModel.removeObserver(mBookmarkModelObserver);
        mEnhancedBookmarksModel.destroy();
        mEnhancedBookmarksModel = null;
        mLargeIconBridge.destroy();
        mLargeIconBridge = null;
    }

    /**
     * Called when the user presses the back key. This is only going to be called on Phone.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        if (doesDrawerExist()) {
            if (mDrawer.isDrawerVisible(Gravity.START)) {
                mDrawer.closeDrawer(Gravity.START);
                return true;
            }
        }

        if (mContentView.onBackPressed()) return true;

        if (!mStateStack.empty()) {
            mStateStack.pop();
            if (!mStateStack.empty()) {
                setState(mStateStack.pop());
                return true;
            }
        }
        return false;
    }

    public View getView() {
        return mMainView;
    }

    /**
     * Sets the listener that reacts upon the change of the UI state of bookmark manager.
     */
    public void setUrlChangeListener(EnhancedBookmarkStateChangeListener urlListner) {
        mUrlChangeListener = urlListner;
    }

    /**
     * @return Current URL representing the UI state of bookmark manager. If no state has been shown
     *         yet in this session, on phone return last used state stored in preference; on tablet
     *         return the url previously set by {@link #updateForUrl(String)}.
     */
    public String getCurrentUrl() {
        if (mStateStack.isEmpty()) return null;
        return mStateStack.peek().mUrl;
    }

    /**
     * Updates UI based on the new URL. If the bookmark model is not loaded yet, cache the url and
     * it will be picked up later when the model is loaded. This method is supposed to align with
     * {@link EnhancedBookmarkPage#updateForUrl(String)}
     * <p>
     * @param url The url to navigate to.
     */
    public void updateForUrl(String url) {
        // Bookmark model is null if the manager has been destroyed.
        if (mEnhancedBookmarksModel == null) return;

        if (mEnhancedBookmarksModel.isBookmarkModelLoaded()) {
            setState(EnhancedBookmarkUIState.createStateFromUrl(url, mEnhancedBookmarksModel));
        } else {
            mInitialUrl = url;
        }
    }

    /**
     * Puts all UI elements to loading state. This state might be overridden synchronously by
     * {@link #updateForUrl(String)}, if the bookmark model is already loaded.
     */
    private void initializeToLoadingState() {
        mContentView.showLoadingUi();
        mDrawerListView.showLoadingUi();
        mContentView.showLoadingUi();
        assert mStateStack.isEmpty();
        setState(EnhancedBookmarkUIState.createLoadingState());
    }

    /**
     * This is the ultimate internal method that updates UI and controls backstack. And it is the
     * only method that pushes states to {@link #mStateStack}.
     * <p>
     * If the given state is not valid, all_bookmark state will be shown. Afterwards, this method
     * checks the current state: if currently in loading state, it pops it out and adds the new
     * state to the back stack. It also notifies the {@link #mUrlChangeListener} (if any) that the
     * url has changed.
     * <p>
     * Also note that even if we store states to {@link #mStateStack}, on tablet the back navigation
     * and back button are not controlled by the manager: the tab handles back key and backstack
     * navigation.
     */
    private void setState(EnhancedBookmarkUIState state) {
        if (!state.isValid(mEnhancedBookmarksModel)) {
            state = EnhancedBookmarkUIState.createAllBookmarksState(mEnhancedBookmarksModel);
        }

        if (!mStateStack.isEmpty() && mStateStack.peek().equals(state)) return;

        // The loading state is not persisted in history stack and once we have a valid state it
        // shall be removed.
        if (!mStateStack.isEmpty()
                && mStateStack.peek().mState == EnhancedBookmarkUIState.STATE_LOADING) {
            mStateStack.pop();
        }
        mStateStack.push(state);

        if (state.mState != EnhancedBookmarkUIState.STATE_LOADING) {
            // Loading state may be pushed to the stack but should never be stored in preferences.
            if (state.mShouldPersist) {
                EnhancedBookmarkUtils.setLastUsedUrl(mActivity, state.mUrl);
            }
            // If a loading state is replaced by another loading state, do not notify this change.
            if (mUrlChangeListener != null) {
                mUrlChangeListener.onBookmarkUIStateChange(state.mUrl);
            }
        }

        clearSelection();

        for (EnhancedBookmarkUIObserver observer : mUIObservers) {
            notifyStateChange(observer);
        }
    }

    // EnhancedBookmarkDelegate implementations.

    @Override
    public boolean isDialogUi() {
        return mIsDialogUi;
    }

    @Override
    public void openFolder(BookmarkId folder) {
        closeSearchUI();
        setState(EnhancedBookmarkUIState.createFolderState(folder, mEnhancedBookmarksModel));
    }

    @Override
    public void openAllBookmarks() {
        closeSearchUI();
        setState(EnhancedBookmarkUIState.createAllBookmarksState(mEnhancedBookmarksModel));
    }

    @Override
    public void openFilter(EnhancedBookmarkFilter filter) {
        setState(EnhancedBookmarkUIState.createFilterState(filter, mEnhancedBookmarksModel));
    }

    @Override
    public void clearSelection() {
        mSelectedBookmarks.clear();
        for (EnhancedBookmarkUIObserver observer : mUIObservers) {
            observer.onSelectionStateChange(new ArrayList<BookmarkId>(mSelectedBookmarks));
        }
    }

    @Override
    public boolean toggleSelectionForBookmark(BookmarkId bookmark) {
        if (!mEnhancedBookmarksModel.getBookmarkById(bookmark).isEditable()) return false;

        if (mSelectedBookmarks.contains(bookmark)) mSelectedBookmarks.remove(bookmark);
        else mSelectedBookmarks.add(bookmark);
        for (EnhancedBookmarkUIObserver observer : mUIObservers) {
            observer.onSelectionStateChange(new ArrayList<BookmarkId>(mSelectedBookmarks));
        }

        return isBookmarkSelected(bookmark);
    }

    @Override
    public boolean isBookmarkSelected(BookmarkId bookmark) {
        return mSelectedBookmarks.contains(bookmark);
    }

    @Override
    public boolean isSelectionEnabled() {
        return !mSelectedBookmarks.isEmpty();
    }

    @Override
    public List<BookmarkId> getSelectedBookmarks() {
        return new ArrayList<BookmarkId>(mSelectedBookmarks);
    }

    @Override
    public void notifyStateChange(EnhancedBookmarkUIObserver observer) {
        int state = getCurrentState();
        switch (state) {
            case EnhancedBookmarkUIState.STATE_ALL_BOOKMARKS:
                observer.onAllBookmarksStateSet();
                break;
            case EnhancedBookmarkUIState.STATE_FOLDER:
                observer.onFolderStateSet(mStateStack.peek().mFolder);
                break;
            case EnhancedBookmarkUIState.STATE_LOADING:
                // In loading state, onEnhancedBookmarkDelegateInitialized() is not called for all
                // UIObservers, which means that there will be no observers at the time. Do nothing.
                assert mUIObservers.isEmpty();
                break;
            case EnhancedBookmarkUIState.STATE_FILTER:
                observer.onFilterStateSet(mStateStack.peek().mFilter);
                break;
            default:
                assert false : "State not valid";
                break;
        }
    }

    @Override
    public boolean doesDrawerExist() {
        return mDrawer != null;
    }

    @Override
    public void closeDrawer() {
        if (!doesDrawerExist()) return;

        mDrawer.closeDrawer(Gravity.START);
    }

    @Override
    public DrawerLayout getDrawerLayout() {
        return mDrawer;
    }

    @Override
    public void openBookmark(BookmarkId bookmark, int launchLocation) {
        clearSelection();
        if (EnhancedBookmarkUtils.openBookmark(
                    mEnhancedBookmarksModel, mActivity, bookmark, launchLocation)) {
            EnhancedBookmarkUtils.finishActivityOnPhone(mActivity);
        }
    }

    @Override
    public void openSearchUI() {
        // Give search view focus, because it needs to handle back key event.
        mViewSwitcher.showNext();
    }

    @Override
    public void closeSearchUI() {
        if (mSearchView.getVisibility() != View.VISIBLE) return;
        mViewSwitcher.showPrevious();
    }

    @Override
    public void addUIObserver(EnhancedBookmarkUIObserver observer) {
        mUIObservers.addObserver(observer);
    }

    @Override
    public void removeUIObserver(EnhancedBookmarkUIObserver observer) {
        mUIObservers.removeObserver(observer);
    }

    @Override
    public EnhancedBookmarksModel getModel() {
        return mEnhancedBookmarksModel;
    }

    @Override
    public int getCurrentState() {
        if (mStateStack.isEmpty()) return EnhancedBookmarkUIState.STATE_LOADING;
        return mStateStack.peek().mState;
    }

    @Override
    public LargeIconBridge getLargeIconBridge() {
        return mLargeIconBridge;
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return ((SnackbarManageable) mActivity).getSnackbarManager();
    }
}
