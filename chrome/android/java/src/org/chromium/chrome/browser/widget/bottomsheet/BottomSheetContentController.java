// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.PointF;
import android.graphics.drawable.Drawable;
import android.support.annotation.IdRes;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.bookmarks.BookmarkSheetContent;
import org.chromium.chrome.browser.download.DownloadSheetContent;
import org.chromium.chrome.browser.history.HistorySheetContent;
import org.chromium.chrome.browser.ntp.IncognitoBottomSheetContent;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.suggestions.SuggestionsBottomSheetContent;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.ViewHighlighter;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.StateChangeReason;
import org.chromium.chrome.browser.widget.bottomsheet.base.BottomNavigationView;
import org.chromium.chrome.browser.widget.bottomsheet.base.BottomNavigationView.OnNavigationItemSelectedListener;
import org.chromium.chrome.browser.widget.textbubble.ViewAnchoredTextBubble;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Displays and controls a {@link BottomNavigationView} fixed to the bottom of the
 * {@link BottomSheet}. Also manages {@link BottomSheetContent} displayed in the BottomSheet.
 */
public class BottomSheetContentController
        extends BottomSheetNavigationView implements OnNavigationItemSelectedListener {
    /** The different types of content that may be displayed in the bottom sheet. */
    @IntDef({TYPE_SUGGESTIONS, TYPE_DOWNLOADS, TYPE_BOOKMARKS, TYPE_HISTORY, TYPE_INCOGNITO_HOME,
            TYPE_AUXILIARY_CONTENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentType {}
    public static final int TYPE_SUGGESTIONS = 0;
    public static final int TYPE_DOWNLOADS = 1;
    public static final int TYPE_BOOKMARKS = 2;
    public static final int TYPE_HISTORY = 3;
    public static final int TYPE_INCOGNITO_HOME = 4;
    // This type should be used for non-primary sheet content. If the sheet is opened because of
    // content with this type, the back button will close the sheet.
    public static final int TYPE_AUXILIARY_CONTENT = 5;

    // Used if there is no bottom sheet content.
    private static final int NO_CONTENT_ID = 0;

    // R.id.action_home is overloaded, so an invalid ID is used to reference the incognito version
    // of the home content.
    private static final int INCOGNITO_HOME_ID = -1;

    // Since the placeholder content cannot be triggered by a navigation item like the others, this
    // value must also be an invalid ID.
    private static final int PLACEHOLDER_ID = -2;

    /** The threshold of application screen height for showing a tall bottom navigation bar. */
    private static final float TALL_BOTTOM_NAV_THRESHOLD_DP = 683.0f;

    /** The height of the bottom navigation bar that appears when the bottom sheet is expanded. */
    private int mBottomNavHeight;

    private final Map<Integer, BottomSheetContent> mBottomSheetContents = new HashMap<>();
    private boolean mLabelsEnabled;
    private boolean mDestroyed;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetOffsetChanged(float heightFraction) {
            // If the omnibox is not focused, allow the navigation bar to set its Y translation.
            if (!mOmniboxHasFocus) {
                float offsetY = mBottomSheet.getSheetHeightForState(mBottomSheet.isSmallScreen()
                                                ? BottomSheet.SHEET_STATE_FULL
                                                : BottomSheet.SHEET_STATE_HALF)
                        - mBottomSheet.getCurrentOffsetPx();
                setTranslationY(Math.max(offsetY, 0f));

                if (mBottomSheet.getTargetSheetState() != BottomSheet.SHEET_STATE_PEEK
                        && mSelectedItemId == PLACEHOLDER_ID) {
                    showBottomSheetContent(R.id.action_home);
                }
            }
            setVisibility(MathUtils.areFloatsEqual(heightFraction, 0f) ? View.GONE : View.VISIBLE);

            mSnackbarManager.dismissAllSnackbars();
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            setIcons();

            if (reason == StateChangeReason.OMNIBOX_FOCUS) {
                // If the omnibox is being focused, show the placeholder.
                mBottomSheet.showContent(mPlaceholderContent);
                mBottomSheet.endTransitionAnimations();
                if (mSelectedItemId > 0) getMenu().findItem(mSelectedItemId).setChecked(false);
                mSelectedItemId = PLACEHOLDER_ID;
                return;
            }

            if (mSelectedItemId == NO_CONTENT_ID) showBottomSheetContent(R.id.action_home);

            if (mHighlightItemId != null) {
                mHighlightedView = mActivity.findViewById(mHighlightItemId);
                ViewHighlighter.turnOnHighlight(mHighlightedView, true);
            }
        }

        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            removeIcons();

            if (ChromeFeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(
                               ChromeFeatureList.CHROME_HOME_DESTROY_SUGGESTIONS)
                    && !ChromeFeatureList.isEnabled(
                               ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_ABOVE_ARTICLES)) {
                // TODO(bauerb): Implement support for destroying the home sheet after a delay.
                mSelectedItemId = NO_CONTENT_ID;
                mBottomSheet.showContent(null);
            } else {
                if (mSelectedItemId != NO_CONTENT_ID && mSelectedItemId != R.id.action_home) {
                    showBottomSheetContent(R.id.action_home);
                } else {
                    clearBottomSheetContents(false);
                }
            }

            // The keyboard should be hidden when the sheet is closed in case it was made visible by
            // sheet content.
            UiUtils.hideKeyboard(BottomSheetContentController.this);

            ViewHighlighter.turnOffHighlight(mHighlightedView);
            mHighlightedView = null;
            mHighlightItemId = null;
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            if (mBottomSheet.isSheetOpen()) announceBottomSheetContentSelected();

            if (mShouldOpenSheetOnNextContentChange) {
                mShouldOpenSheetOnNextContentChange = false;
                @BottomSheet.SheetState
                int targetState = mShouldOpenSheetToHalfOnNextContentChange
                        ? BottomSheet.SHEET_STATE_HALF
                        : BottomSheet.SHEET_STATE_FULL;
                if (mBottomSheet.getSheetState() != targetState) {
                    mBottomSheet.setSheetState(targetState, true);
                }
                return;
            }

            // If the home content is showing and the sheet is closed, destroy sheet contents that
            // are no longer needed.
            if ((mShouldClearContentsOnNextContentChange
                        || mBottomSheet.getSheetState() == BottomSheet.SHEET_STATE_PEEK)
                    && (newContent == null
                               || newContent == mBottomSheetContents.get(getHomeContentId()))) {
                clearBottomSheetContents(newContent == null);
            }
        }
    };

    private BottomSheet mBottomSheet;
    private TabModelSelector mTabModelSelector;
    private SnackbarManager mSnackbarManager;
    private int mSelectedItemId;
    private ChromeActivity mActivity;
    private boolean mShouldOpenSheetOnNextContentChange;
    private boolean mShouldOpenSheetToHalfOnNextContentChange;
    private boolean mShouldClearContentsOnNextContentChange;
    private PlaceholderSheetContent mPlaceholderContent;
    private boolean mOmniboxHasFocus;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private Integer mHighlightItemId;
    private View mHighlightedView;
    private boolean mNavItemSelectedWhileOmniboxFocused;
    private int mConsecutiveBookmarkTaps;

    public BottomSheetContentController(Context context, AttributeSet atts) {
        super(context, atts);

        mPlaceholderContent = new PlaceholderSheetContent(context);
    }

    public void setHighlightItemId(@Nullable Integer highlightItemId) {
        mHighlightItemId = highlightItemId;
    }

    /** Called when the activity containing the bottom sheet is destroyed. */
    public void destroy() {
        mDestroyed = true;
        clearBottomSheetContents(true);
        if (mPlaceholderContent != null) {
            mPlaceholderContent.destroy();
            mPlaceholderContent = null;
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelector = null;
        }
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                .addStartupCompletedObserver(new BrowserStartupController.StartupCallback() {
                    @Override
                    public void onSuccess(boolean alreadyStarted) {
                        initBottomNavMenu();
                        setIconsEnabled(true);
                    }

                    @Override
                    public void onFailure() {}
                });
    }

    /**
     * Initializes the {@link BottomSheetContentController}.
     * @param bottomSheet The {@link BottomSheet} associated with this bottom nav.
     * @param tabModelSelector The {@link TabModelSelector} for the application.
     * @param activity The {@link ChromeActivity} that owns the BottomSheet.
     */
    public void init(
            BottomSheet bottomSheet, TabModelSelector tabModelSelector, ChromeActivity activity) {
        mBottomSheet = bottomSheet;
        mBottomSheet.addObserver(mBottomSheetObserver);
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        setIconsEnabled(mActivity.didFinishNativeInitialization());

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                // Remove all bottom sheet contents except Home if switching between standard and
                // incognito mode to prevent contents from being out of date.
                if (newModel.isIncognito() != oldModel.isIncognito()) {
                    mShouldClearContentsOnNextContentChange = true;
                }

                updateVisuals(newModel.isIncognito());
                showBottomSheetContent(R.id.action_home);
                mPlaceholderContent.setIsIncognito(newModel.isIncognito());

                // Release incognito bottom sheet content so that it can be garbage collected.
                if (!newModel.isIncognito()
                        && mBottomSheetContents.containsKey(INCOGNITO_HOME_ID)) {
                    mBottomSheetContents.get(INCOGNITO_HOME_ID).destroy();
                    mBottomSheetContents.remove(INCOGNITO_HOME_ID);
                }
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        setOnNavigationItemSelectedListener(this);

        ViewGroup snackbarContainer =
                (ViewGroup) activity.findViewById(R.id.bottom_sheet_snackbar_container);

        mSnackbarManager = new SnackbarManager(mActivity, snackbarContainer);
        mSnackbarManager.onStart();

        ApplicationStatus.registerStateListenerForActivity(new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.STARTED) mSnackbarManager.onStart();
                if (newState == ActivityState.STOPPED) mSnackbarManager.onStop();
            }
        }, mActivity);

        // We use a global layout listener here to ensure we update menu item spacing after the
        // menu icons have their full width.
        mBottomSheet.getViewTreeObserver().addOnGlobalLayoutListener(new OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                updateMenuItemSpacing();
            }
        });

        updateVisuals(mTabModelSelector.isIncognitoSelected());
        BottomSheetPaddingUtils.applyPaddingToContent(mPlaceholderContent, mBottomSheet);
    }

    /**
     * Initializes the height of the bottom navigation menu based on the device's screen dp density,
     * and whether or not we're showing labels beneath the icons in the menu.
     *
     * Needs to be called after the native library is loaded.
     */
    private void initBottomNavMenu() {
        assert ChromeFeatureList.isInitialized();

        DisplayMetrics metrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        boolean useTallBottomNav =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME_BOTTOM_NAV_LABELS)
                || Float.compare(
                           Math.max(metrics.heightPixels, metrics.widthPixels) / metrics.density,
                           TALL_BOTTOM_NAV_THRESHOLD_DP)
                        >= 0;
        mBottomNavHeight = getResources().getDimensionPixelSize(
                useTallBottomNav ? R.dimen.bottom_nav_height_tall : R.dimen.bottom_nav_height);

        initializeMenuView();
    }

    /**
     * Initializes the menu, hiding labels and showing the shadow if necessary, and centering menu
     * items.
     *
     * Also takes care of correctly positioning the snackbar above the menu view.
     *
     * Needs to be called after the native library is loaded.
     */
    private void initializeMenuView() {
        if (mDestroyed) return;

        mLabelsEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME_BOTTOM_NAV_LABELS);
        if (mLabelsEnabled) {
            ImageView bottomNavShadow = (ImageView) findViewById(R.id.bottom_nav_shadow);
            if (bottomNavShadow != null) bottomNavShadow.setVisibility(View.VISIBLE);
        } else {
            hideMenuLabels();
        }

        // Since the BottomSheetContentController may also include a shadow, we need to ensure
        // that the menu gets adjusted to the appropriate height, and that we include the height
        // of the shadow in the controller's height.
        int bottomNavShadowHeight = mLabelsEnabled
                ? mActivity.getResources().getDimensionPixelSize(R.dimen.bottom_nav_shadow_height)
                : 0;
        getLayoutParams().height = mBottomNavHeight + bottomNavShadowHeight;
        getMenuView().getLayoutParams().height = mBottomNavHeight;
        getMenuView().getLayoutParams().width = LayoutParams.MATCH_PARENT;
        getMenuView().setGravity(Gravity.CENTER);

        ViewGroup snackbarContainer =
                (ViewGroup) mActivity.findViewById(R.id.bottom_sheet_snackbar_container);
        ((MarginLayoutParams) snackbarContainer.getLayoutParams()).bottomMargin = mBottomNavHeight;

        setMenuBackgroundColor(
                mTabModelSelector != null ? mTabModelSelector.isIncognitoSelected() : false);
    }

    /**
     * A helper function to set the menu view background color.
     *
     * @param isIncognitoTabModelSelected Whether or not we're using incognito mode.
     */
    private void setMenuBackgroundColor(boolean isIncognitoTabModelSelected) {
        getMenuView().setBackgroundResource(
                getBackgroundColorResource(isIncognitoTabModelSelected));
    }

    /**
     * Whenever this is triggered by a global layout change, we ensure that our bottom navigation
     * menu items are spaced apart appropriately.
     */
    private void updateMenuItemSpacing() {
        getMenuView().updateMenuItemSpacingForMinWidth(
                mBottomSheet.getWidth(), mBottomSheet.getHeight());
    }

    /**
     * @param itemId The id of the MenuItem to select.
     */
    public void selectItem(int itemId) {
        // TODO(twellington): A #setSelectedItemId() method was added to the support library
        //                    recently. Replace this custom implementation with that method after
        //                    the support library is rolled.
        onNavigationItemSelected(getMenu().findItem(itemId));
    }

    /**
     * Shows the specified {@link BottomSheetContent} and opens the {@link BottomSheet}.
     * @param itemId The menu item id of the {@link BottomSheetContent} to show.
     */
    public void showContentAndOpenSheet(int itemId) {
        if (mActivity.isInOverviewMode() && !mBottomSheet.isShowingNewTab()) {
            // Open a new tab to show the content if currently in tab switcher and a new tab is
            // not currently being displayed.
            mShouldOpenSheetOnNextContentChange = true;
            mBottomSheet.displayNewTabUi(mTabModelSelector.getCurrentModel().isIncognito(), itemId);
        } else if (itemId != mSelectedItemId) {
            mShouldOpenSheetOnNextContentChange = true;
            selectItem(itemId);
        } else if (mBottomSheet.getSheetState() != BottomSheet.SHEET_STATE_FULL) {
            mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_FULL, true);
        }
    }

    /**
     * A notification that the omnibox focus state is changing.
     * @param hasFocus Whether or not the omnibox has focus.
     */
    public void onOmniboxFocusChange(boolean hasFocus) {
        mOmniboxHasFocus = hasFocus;

        if (!mNavItemSelectedWhileOmniboxFocused && !mOmniboxHasFocus) {
            showBottomSheetContent(R.id.action_home);
        }
        mNavItemSelectedWhileOmniboxFocused = false;
    }

    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        mConsecutiveBookmarkTaps =
                item.getItemId() == R.id.action_bookmarks ? mConsecutiveBookmarkTaps + 1 : 0;
        if (mConsecutiveBookmarkTaps >= 5) {
            mConsecutiveBookmarkTaps = 0;
            doStarExplosion((ViewGroup) getRootView(), findViewById(R.id.action_bookmarks));
        }

        if (mOmniboxHasFocus) mNavItemSelectedWhileOmniboxFocused = true;

        if (mBottomSheet.getSheetState() == BottomSheet.SHEET_STATE_PEEK
                && !mShouldOpenSheetOnNextContentChange) {
            return false;
        }

        ViewHighlighter.turnOffHighlight(mHighlightedView);
        mHighlightedView = null;
        mHighlightItemId = null;

        boolean isShowingAuxContent = mBottomSheet.getCurrentSheetContent() != null
                && mBottomSheet.getCurrentSheetContent().getType() == TYPE_AUXILIARY_CONTENT;

        if (mSelectedItemId == item.getItemId() && !isShowingAuxContent) {
            getSheetContentForId(mSelectedItemId).scrollToTop();
            return false;
        }

        mBottomSheet.defocusOmnibox();

        mSnackbarManager.dismissAllSnackbars();
        showBottomSheetContent(item.getItemId());
        return true;
    }

    private void hideMenuLabels() {
        BottomSheetNavigationMenuView menuView = getMenuView();
        for (int i = 0; i < menuView.getChildCount(); i++) {
            BottomSheetNavigationItemView item =
                    (BottomSheetNavigationItemView) menuView.getChildAt(i);
            item.hideLabel();
        }
    }

    private BottomSheetContent getSheetContentForId(int navItemId) {
        if (mTabModelSelector.isIncognitoSelected() && navItemId == R.id.action_home) {
            navItemId = INCOGNITO_HOME_ID;
        }

        BottomSheetContent content = mBottomSheetContents.get(navItemId);
        if (content != null) return content;

        return createAndCacheContentForId(navItemId);
    }

    private int getHomeContentId() {
        if (mTabModelSelector.isIncognitoSelected()) return INCOGNITO_HOME_ID;
        return R.id.action_home;
    }

    /**
     * Create and return the content for the provided nav button id.
     * @param navItemId The id to create and get the content for.
     * @return The created content.
     */
    private BottomSheetContent createAndCacheContentForId(int navItemId) {
        BottomSheetContent content = null;
        if (navItemId == R.id.action_home) {
            content = new SuggestionsBottomSheetContent(
                    mActivity, mBottomSheet, mTabModelSelector, mSnackbarManager);
        } else if (navItemId == R.id.action_downloads) {
            content = new DownloadSheetContent(
                    mActivity, mTabModelSelector.getCurrentModel().isIncognito(), mSnackbarManager);
        } else if (navItemId == R.id.action_bookmarks) {
            content = new BookmarkSheetContent(mActivity, mSnackbarManager);
        } else if (navItemId == R.id.action_history) {
            content = new HistorySheetContent(mActivity, mSnackbarManager);
        } else if (navItemId == INCOGNITO_HOME_ID) {
            content = new IncognitoBottomSheetContent(mActivity);
        }

        // Call this only after it's created to avoid adding an infinite amount of additional
        // padding.
        BottomSheetPaddingUtils.applyPaddingToContent(content, mBottomSheet);

        mBottomSheetContents.put(navItemId, content);
        return content;
    }

    private void showBottomSheetContent(int navItemId) {
        // There are some bugs related to programmatically selecting menu items that are fixed in
        // newer support library versions.
        // TODO(twellington): remove this after the support library is rolled.
        if (mSelectedItemId > 0) getMenu().findItem(mSelectedItemId).setChecked(false);
        mSelectedItemId = navItemId;
        getMenu().findItem(mSelectedItemId).setChecked(true);

        // Cancel clearing contents for switched tab model if showing contents other than Home.
        // This is to prevent crash when bottom nav is clicked shortly after switching tab model.
        // See crbug.com/780430 for more details.
        if (mShouldClearContentsOnNextContentChange && mSelectedItemId != getHomeContentId()) {
            mShouldClearContentsOnNextContentChange = false;
        }

        BottomSheetContent newContent = getSheetContentForId(mSelectedItemId);
        mBottomSheet.showContent(newContent);
    }

    private void announceBottomSheetContentSelected() {
        if (mSelectedItemId == R.id.action_home) {
            announceForAccessibility(getResources().getString(R.string.bottom_sheet_home_tab));
        } else if (mSelectedItemId == R.id.action_downloads) {
            announceForAccessibility(getResources().getString(R.string.bottom_sheet_downloads_tab));
        } else if (mSelectedItemId == R.id.action_bookmarks) {
            announceForAccessibility(getResources().getString(R.string.bottom_sheet_bookmarks_tab));
        } else if (mSelectedItemId == R.id.action_history) {
            announceForAccessibility(getResources().getString(R.string.bottom_sheet_history_tab));
        }
    }

    private void updateVisuals(boolean isIncognitoTabModelSelected) {
        setMenuBackgroundColor(isIncognitoTabModelSelected);

        ColorStateList tint = ApiCompatibilityUtils.getColorStateList(getResources(),
                isIncognitoTabModelSelected ? R.color.bottom_nav_tint_incognito
                                            : R.color.bottom_nav_tint);

        setItemIconTintList(tint);
        setItemTextColor(tint);
    }

    private void setIcons() {
        getMenu().findItem(R.id.action_home).setIcon(R.drawable.ic_home_grey600_24dp);
        getMenu().findItem(R.id.action_downloads).setIcon(R.drawable.ic_file_download_white_24dp);
        getMenu().findItem(R.id.action_bookmarks).setIcon(R.drawable.btn_star_filled);
        getMenu().findItem(R.id.action_history).setIcon(R.drawable.ic_watch_later_24dp);
    }

    private void removeIcons() {
        getMenu().findItem(R.id.action_home).setIcon(null);
        getMenu().findItem(R.id.action_downloads).setIcon(null);
        getMenu().findItem(R.id.action_bookmarks).setIcon(null);
        getMenu().findItem(R.id.action_history).setIcon(null);
    }

    private void setIconsEnabled(boolean enabled) {
        getMenu().findItem(R.id.action_downloads).setEnabled(enabled);
        getMenu().findItem(R.id.action_bookmarks).setEnabled(enabled);
        getMenu().findItem(R.id.action_history).setEnabled(enabled);
    }

    /**
     * Get the background color resource ID for the bottom navigation menu based on whether
     * we're in incognito mode.
     *
     * Falls back to the common default if the feature list hasn't been initialized yet, which is
     * the case on the initial layout pass before the native libraries are loaded.
     *
     * @param isIncognitoTabModelSelected Is the incognito tab model selected.
     * @return The Android resource ID for the background color.
     */
    private int getBackgroundColorResource(boolean isIncognitoTabModelSelected) {
        if (isIncognitoTabModelSelected) {
            return mLabelsEnabled ? R.color.incognito_primary_color
                                  : R.color.incognito_primary_color_home_bottom_nav;
        }
        return mLabelsEnabled ? R.color.modern_primary_color
                              : R.color.primary_color_home_bottom_nav;
    }

    @VisibleForTesting
    public int getSelectedItemIdForTests() {
        // TODO(twellington): A #getSelectedItemId() method was added to the support library
        //                    recently. Replace this custom implementation with that method after
        //                    the support library is rolled.
        return mSelectedItemId;
    }

    private void clearBottomSheetContents(boolean destroyHomeContent) {
        Iterator<Entry<Integer, BottomSheetContent>> contentIterator =
                mBottomSheetContents.entrySet().iterator();
        while (contentIterator.hasNext()) {
            Entry<Integer, BottomSheetContent> entry = contentIterator.next();
            if (!destroyHomeContent
                    && (entry.getKey() == R.id.action_home
                               || entry.getKey() == INCOGNITO_HOME_ID)) {
                continue;
            }

            entry.getValue().destroy();
            contentIterator.remove();
        }
    }

    /**
     * Gets the bottom navigation menu height in pixels.
     * @return Bottom navigation menu height in pixels.
     */
    public int getBottomNavHeight() {
        return mBottomNavHeight;
    }

    /**
     * Explode some stars from the center of the bookmarks icon.
     * @param rootView The root view to run in.
     * @param selectedItemView The item that was selected.
     */
    private void doStarExplosion(ViewGroup rootView, View selectedItemView) {
        if (rootView == null || selectedItemView == null) return;

        Drawable starFull =
                ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.btn_star_filled);
        Drawable starEmpty = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.btn_star);

        int[] outPosition = new int[2];
        ViewUtils.getRelativeDrawPosition(rootView, selectedItemView, outPosition);

        // Center the star's start position over the icon in the middle of the button.
        outPosition[0] += selectedItemView.getWidth() / 2.f - starFull.getIntrinsicWidth() / 2.f;
        outPosition[1] += selectedItemView.getHeight() / 2.f - starFull.getIntrinsicHeight() / 2.f;

        for (int i = 0; i < 5; i++) {
            MagicStar star = new MagicStar(getContext(), outPosition[0], outPosition[1], 100 * i,
                    Math.random() > 0.5f ? starFull : starEmpty);
            rootView.addView(star);
        }
    }

    /**
     * This is an image view that runs a simple animation before detaching itself from the window.
     */
    private static class MagicStar extends ImageView {
        /** The starting position of the star. */
        private PointF mStartPosition;

        /** The direction of the star. */
        private PointF mVector;

        /** The velocity of the star. */
        private float mVelocity;

        /** The speed and direction that the star is rotating. Negative is counter-clockwise. */
        private float mRotationVelocity;

        /** The animation delay for the star. */
        private long mStartDelay;

        public MagicStar(
                Context context, float startX, float startY, long startDelay, Drawable drawable) {
            super(context);

            mStartPosition = new PointF(startX, startY);
            mStartDelay = startDelay;

            // Fire stars within 45 degrees of 'up'.
            float minAngle = (float) Math.toRadians(45.f);
            float maxAngle = (float) Math.toRadians(135.f);
            mVector = new PointF((float) Math.cos(getRandomInInterval(minAngle, maxAngle)),
                    (float) Math.sin(getRandomInInterval(minAngle, maxAngle)));

            mVelocity = getRandomInInterval(10.f, 15.f);
            mRotationVelocity = getRandomInInterval(-2.f, 2.f);

            setImageDrawable(drawable);
        }

        /**
         * Get a random number between min and max.
         * @param min The minimum value of the float.
         * @param max The maximum value of the float.
         * @return A random number between min and max.
         */
        private float getRandomInInterval(float min, float max) {
            return (float) (Math.random() * (max - min)) + min;
        }

        @Override
        public void onAttachedToWindow() {
            super.onAttachedToWindow();

            setAlpha(0.f);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    getDrawable().getIntrinsicWidth(), getDrawable().getIntrinsicHeight());
            setLayoutParams(params);

            setTranslationX(mStartPosition.x);
            setTranslationY(mStartPosition.y);

            doAnimation();
        }

        /**
         * Run the star's animation and detach it from the window when complete.
         */
        private void doAnimation() {
            ValueAnimator animator = ValueAnimator.ofFloat(0.f, 1.f);
            animator.setDuration(500);
            animator.setStartDelay(mStartDelay);

            animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator valueAnimator) {
                    float value = (float) valueAnimator.getAnimatedValue();
                    setAlpha(1.f - value);
                    setTranslationX(getTranslationX() - mVector.x * mVelocity);
                    setTranslationY(getTranslationY() - mVector.y * mVelocity);
                    setScaleX(0.4f + value);
                    setScaleY(0.4f + value);
                    setRotation((getRotation() + mRotationVelocity) % 360);
                }
            });

            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationStart(Animator animation) {
                    setAlpha(1.f);
                }

                @Override
                public void onAnimationEnd(Animator animation) {
                    ((ViewGroup) getParent()).removeView(MagicStar.this);
                }
            });

            animator.start();
        }
    }

    /**
     * Record a menu item click and open the bottom sheet to the specified content. This method
     * assumes that Chrome Home is enabled.
     * @param navId The bottom sheet navigation id to open.
     */
    public void openBottomSheetForMenuItem(@IdRes int navId) {
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            ChromePreferenceManager.getInstance().incrementChromeHomeMenuItemClickCount();
        }

        mShouldOpenSheetToHalfOnNextContentChange = true;
        final View highlightedView = findViewById(navId);

        int stringId = 0;
        if (navId == R.id.action_bookmarks) {
            stringId = R.string.chrome_home_menu_bookmarks_help_bubble;
        } else if (navId == R.id.action_downloads) {
            stringId = R.string.chrome_home_menu_downloads_help_bubble;
        } else if (navId == R.id.action_history) {
            stringId = R.string.chrome_home_menu_history_help_bubble;
        } else {
            throw new RuntimeException("Attempting to show invalid content in bottom sheet.");
        }

        final ViewAnchoredTextBubble helpBubble =
                new ViewAnchoredTextBubble(getContext(), mBottomSheet, stringId, stringId);
        helpBubble.setDismissOnTouchInteraction(true);

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            /** Whether the help bubble has been shown. */
            private boolean mHelpBubbleShown;

            @Override
            public void onSheetContentChanged(BottomSheet.BottomSheetContent newContent) {
                if (getSheetContentForId(navId) == newContent) return;
                post(() -> {
                    ViewHighlighter.turnOffHighlight(highlightedView);
                    mBottomSheet.removeObserver(this);
                });
            }

            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                ViewHighlighter.turnOnHighlight(highlightedView, false);
                mShouldOpenSheetToHalfOnNextContentChange = false;
            }

            @Override
            public void onSheetStateChanged(@BottomSheet.SheetState int state) {
                if (state != BottomSheet.SHEET_STATE_HALF || mHelpBubbleShown) return;
                int inset = getContext().getResources().getDimensionPixelSize(
                        R.dimen.bottom_sheet_help_bubble_inset);
                helpBubble.setInsetPx(0, inset, 0, inset);
                helpBubble.show();
                mHelpBubbleShown = true;
            }
        });

        showContentAndOpenSheet(navId);
    }
}
