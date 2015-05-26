// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.Editable;
import android.text.Layout;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.text.TextWatcher;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.ViewSwitcher;

import com.google.android.apps.chrome.R;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.FiltersObserver;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksModel;
import org.chromium.chrome.browser.enhanced_bookmarks.LaunchLocation;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkSalientImageView.SalientImageDrawableFactory;
import org.chromium.chrome.browser.widget.CustomShapeDrawable.CircularDrawable;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkMatch;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * UI implementation for search in enhanced bookmark. Search results will be updated when user is
 * typing. Before typing, a list of search history is shown.
 */
public class EnhancedBookmarkSearchView extends LinearLayout implements View.OnClickListener,
        OnItemClickListener, FiltersObserver, EnhancedBookmarkUIObserver {

    private static enum UIState {HISTORY, RESULT, EMPTY}
    private static final int HISTORY_ITEM_PADDING_START_DP = 72;
    private static final int MAXIMUM_NUMBER_OF_RESULTS = 500;
    private static final String HORIZONTAL_ELLIPSIS = "\u2026";
    private EnhancedBookmarksModel mEnhancedBookmarksModel;
    private EnhancedBookmarkDelegate mDelegate;
    private ImageButton mBackButton;
    private EditText mSearchText;
    private ListView mResultList;
    private ListView mHistoryList;
    private HistoryResultSwitcher mHistoryResultSwitcher;
    private UIState mCurrentUIState;

    private BookmarkModelObserver mModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            if (mCurrentUIState == UIState.RESULT || mCurrentUIState == UIState.EMPTY) {
                sendLocalSearchQuery();
            }
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            // If isDoingExtensiveChanges is false, it will fall back to bookmarkModelChange()
            if (isDoingExtensiveChanges && mCurrentUIState == UIState.RESULT) {
                sendLocalSearchQuery();
            }
        }
    };

    /**
     * Constructor for inflating from XML.
     */
    public EnhancedBookmarkSearchView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBackButton = (ImageButton) findViewById(R.id.eb_search_back);
        mSearchText = (EditText) findViewById(R.id.eb_search_text);
        mResultList = (ListView) findViewById(R.id.eb_result_list);
        mHistoryList = (ListView) findViewById(R.id.eb_history_list);
        mHistoryResultSwitcher = (HistoryResultSwitcher) findViewById(R.id.history_result_switcher);

        mHistoryList.setOnItemClickListener(this);
        mResultList.setOnItemClickListener(this);
        mBackButton.setOnClickListener(this);
        mSearchText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (TextUtils.isEmpty(s.toString().trim())) {
                    resetUI();
                } else {
                    sendLocalSearchQuery();
                }
            }
        });
        mCurrentUIState = UIState.HISTORY;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mHistoryList && view instanceof TextView) {
            List<BookmarkId> bookmarkIds = mEnhancedBookmarksModel
                    .getBookmarksForFilter((String) parent.getAdapter().getItem(position));
            List<BookmarkMatch> bookmarkMatches = new ArrayList<BookmarkMatch>();
            for (BookmarkId bookmarkId : bookmarkIds) {
                bookmarkMatches.add(new BookmarkMatch(bookmarkId, null, null));
            }
            populateResultListView(bookmarkMatches);
        } else if (parent == mResultList) {
            mDelegate.openBookmark(
                    ((BookmarkMatch) parent.getAdapter().getItem(position)).getBookmarkId(),
                    LaunchLocation.SEARCH);
            mDelegate.closeSearchUI();
        }
    }

    private void resetUI() {
        setUIState(UIState.HISTORY);
        mResultList.setAdapter(null);
        if (!mSearchText.getText().toString().isEmpty()) mSearchText.setText("");
    }

    private void sendLocalSearchQuery() {
        String currentText = mSearchText.getText().toString().trim();
        if (TextUtils.isEmpty(currentText)) return;

        List<BookmarkMatch> results = mEnhancedBookmarksModel.getLocalSearchForQuery(currentText,
                MAXIMUM_NUMBER_OF_RESULTS);
        if (results != null) populateResultListView(results);
    }

    /**
     * Make result list visible and popuplate the list with given list of bookmarks.
     */
    private void populateResultListView(List<BookmarkMatch> ids) {
        if (ids.isEmpty()) {
            setUIState(UIState.EMPTY);
        } else {
            setUIState(UIState.RESULT);
            mResultList.setAdapter(new ResultListAdapter(ids, mEnhancedBookmarksModel));
        }
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        // This method might be called very early. Null check on bookmark model here.
        if (mEnhancedBookmarksModel == null) return;

        if (visibility != View.VISIBLE) {
            UiUtils.hideKeyboard(mSearchText);
            mEnhancedBookmarksModel.removeFiltersObserver(this);
            mEnhancedBookmarksModel.removeModelObserver(mModelObserver);
            resetUI();
            clearFocus();
        } else {
            mEnhancedBookmarksModel.addModelObserver(mModelObserver);
            mEnhancedBookmarksModel.addFiltersObserver(this);
            onFiltersChanged();
            mSearchText.requestFocus();
            UiUtils.showKeyboard(mSearchText);
        }
    }

    private void setUIState(UIState state) {
        if (mCurrentUIState == state) return;
        mCurrentUIState = state;
        if (state == UIState.HISTORY) {
            mHistoryResultSwitcher.showHistory();
        } else if (state == UIState.RESULT) {
            mHistoryResultSwitcher.showResult();
        } else if (state == UIState.EMPTY) {
            mHistoryResultSwitcher.showEmpty();
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // To intercept hardware key, a view must have focus.
        if (mDelegate == null) return super.dispatchKeyEvent(event);

        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            KeyEvent.DispatcherState state = getKeyDispatcherState();
            if (state != null) {
                if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                    state.startTracking(event, this);
                    return true;
                } else if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()
                        && state.isTracking(event)) {
                    onClick(mBackButton);
                    return true;
                }
            }
        }

        return super.dispatchKeyEvent(event);
    }

    /**
     * Click listener for back button.
     */
    @Override
    public void onClick(View v) {
        assert v == mBackButton;
        if (mCurrentUIState == UIState.HISTORY) {
            mDelegate.closeSearchUI();
        } else {
            resetUI();
        }
    }

    @Override
    public void onFiltersChanged() {
        // We use android default list and textviews for history. Only start padding is modified.
        mHistoryList.setAdapter(new ArrayAdapter<String>(getContext(),
                android.R.layout.simple_list_item_1, android.R.id.text1,
                mEnhancedBookmarksModel.getFilters()) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View textView = super.getView(position, convertView, parent);
                // Set padding start to specific size.
                int paddingStart = (int) (HISTORY_ITEM_PADDING_START_DP
                        * getResources().getDisplayMetrics().density);
                ApiCompatibilityUtils.setPaddingRelative(textView, paddingStart,
                        textView.getPaddingTop(), textView.getPaddingRight(),
                        textView.getPaddingBottom());
                return textView;
            }
        });
    }

    // EnhancedBookmarkUIObserver implementation

    @Override
    public void onEnhancedBookmarkDelegateInitialized(EnhancedBookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mEnhancedBookmarksModel = mDelegate.getModel();
    }

    @Override
    public void onDestroy() {
        mEnhancedBookmarksModel.removeFiltersObserver(this);
        mEnhancedBookmarksModel.removeModelObserver(mModelObserver);
        mDelegate.removeUIObserver(this);
    }

    @Override
    public void onAllBookmarksStateSet() {
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
    }

    @Override
    public void onFilterStateSet(String filter) {
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
    }

    @Override
    public void onListModeChange(boolean isListModeEnabled) {}

    /**
     * List Adapter that organizes search results.
     */
    private static class ResultListAdapter extends BaseAdapter implements
            SalientImageDrawableFactory {

        private EnhancedBookmarksModel mBookmarksModel;
        private List<BookmarkMatch> mResultList;

        public ResultListAdapter(List<BookmarkMatch> bookmarkMatches,
                EnhancedBookmarksModel enhancedBookmarksModel) {
            mBookmarksModel = enhancedBookmarksModel;
            mResultList = bookmarkMatches;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final BookmarkMatch bookmarkMatch = getItem(position);
            BookmarkItem bookmarkItem = mBookmarksModel.getBookmarkById(
                    bookmarkMatch.getBookmarkId());
            if (convertView == null) {
                convertView = LayoutInflater.from(parent.getContext()).inflate(
                        R.layout.eb_search_result_item, parent, false);
            }
            final TextView titleTextView = (TextView) convertView.findViewById(R.id.title);
            final TextView urlTextView = (TextView) convertView.findViewById(R.id.url);
            EnhancedBookmarkSalientImageView imageView = (EnhancedBookmarkSalientImageView)
                    convertView.findViewById(R.id.round_image);
            imageView.load(mBookmarksModel, bookmarkItem.getUrl(),
                    EnhancedBookmarkUtils.generateBackgroundColor(bookmarkItem), this);

            final SpannableString title = boldMatchPositions(bookmarkItem.getTitle(),
                    bookmarkMatch.getTitleMatchPositions());
            final SpannableString url = boldMatchPositions(bookmarkItem.getUrl(),
                    bookmarkMatch.getUrlMatchPositions());

            titleTextView.setText(title);
            urlTextView.setText(url);

            // Ensure the first matched search term is visible on the screen by calling
            // moveCharactersOnScreen after the titleTextView.onLayout() has been executed
            if (bookmarkMatch.getTitleMatchPositions() != null
                    && bookmarkMatch.getTitleMatchPositions().size() > 0) {
                titleTextView.addOnLayoutChangeListener(new OnLayoutChangeListener(){
                    @Override
                    public void onLayoutChange(View v, int left, int top, int right, int bottom,
                            int oldLeft, int oldTop, int oldRight, int oldBottom) {
                        moveCharactersOnScreen(titleTextView, title,
                                bookmarkMatch.getTitleMatchPositions().get(0).first,
                                bookmarkMatch.getTitleMatchPositions().get(0).second);
                    }
                });
            }

            // Ensure the first matched search term is visible on the screen by calling
            // moveCharactersOnScreen after the urlTextView.onLayout() has been executed
            if (bookmarkMatch.getUrlMatchPositions() != null
                    && bookmarkMatch.getUrlMatchPositions().size() > 0) {
                urlTextView.addOnLayoutChangeListener(new OnLayoutChangeListener(){
                    @Override
                    public void onLayoutChange(View v, int left, int top, int right, int bottom,
                            int oldLeft, int oldTop, int oldRight, int oldBottom) {
                        moveCharactersOnScreen(urlTextView, url,
                                bookmarkMatch.getUrlMatchPositions().get(0).first,
                                bookmarkMatch.getUrlMatchPositions().get(0).second);
                    }
                });
            }

            return convertView;
        }

        private SpannableString boldMatchPositions(String input,
                List<Pair<Integer, Integer>> matches) {
            SpannableString output = new SpannableString(input);
            if (matches != null) {
                for (Pair<Integer, Integer> match : matches) {
                    StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
                    output.setSpan(boldSpan, match.first, match.second, 0);
                }
            }

            return output;
        }

        /**
         * Ensures the characters from [charStartPos, charEndPos) are on screen.
         *
         * @param textView The textView to manipulate
         * @param viewText A SpannableString containing the textView's text
         * @param charStartPos The starting position for characters that should be on screen
         * @param charEndPos The ending position for characters that should be on screen
         */
        private void moveCharactersOnScreen(TextView textView, SpannableString viewText,
                int charStartPos, int charEndPos) {
            if (textView.getLayout() == null) return;

            if (!isCharacterOnScreen(textView.getLayout(), charEndPos - 1)) {
                int subStringLength = textView.getLayout().getEllipsisStart(0);
                int termLength = charEndPos - charStartPos;
                int subStringStartPos = charStartPos - ((subStringLength - termLength) / 2);

                if (subStringStartPos + subStringLength > viewText.length()) {
                    // Characters are near the end of viewText, ellipsize the start
                    textView.setEllipsize(TruncateAt.START);
                } else {
                    // Characters are somwhere in the middle of the viewText, change the textView's
                    // text so the characters are on screen; both sides of the text will be
                    // ellipsized
                    SpannableStringBuilder newText = new SpannableStringBuilder(viewText,
                            subStringStartPos, viewText.length());
                    newText.insert(0, HORIZONTAL_ELLIPSIS);
                    textView.setText(newText);
                }
            }
        }

        private boolean isCharacterOnScreen(Layout textViewLayout, int charPosition) {
            int ellipsisPosition = textViewLayout.getEllipsisStart(0);
            return ellipsisPosition == 0 || charPosition < ellipsisPosition;
        }

        @Override
        public int getCount() {
            return mResultList.size();
        }

        @Override
        public BookmarkMatch getItem(int position) {
            return mResultList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        // SalientImageDrawableFactory implementation

        @Override
        public Drawable getSalientDrawable(int color) {
            return new CircularDrawable(color);
        }

        @Override
        public Drawable getSalientDrawable(Bitmap bitmap) {
            return new CircularDrawable(bitmap);
        }
    }

    /**
     * A custom {@link ViewSwitcher} that makes specific assumptions about the corresponding xml. It
     * assumes it has two children: a {@link ListView} and a {@link ViewSwitcher}, and in the other
     * {@link ViewSwitcher}, it contains a {@link TextView} and a {@link ListView}.
     */
    public static class HistoryResultSwitcher extends ViewSwitcher {
        ViewSwitcher mResultEmptySwitcher;

        /**
         * Constructor for xml inflation.
         */
        public HistoryResultSwitcher(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onFinishInflate() {
            super.onFinishInflate();
            mResultEmptySwitcher = (ViewSwitcher) findViewById(R.id.result_empty_switcher);
        }

        void showHistory() {
            if (getCurrentView().getId() == R.id.eb_history_list) return;
            showNext();
        }

        void showResult() {
            // If currently showing history, toggle.
            if (getCurrentView().getId() == R.id.eb_history_list) showNext();
            // If currently showing empty view, toggle.
            if (mResultEmptySwitcher.getCurrentView().getId() == R.id.eb_search_empty_view) {
                mResultEmptySwitcher.showNext();
            }
        }

        void showEmpty() {
            // If currently showing history, toggle.
            if (getCurrentView().getId() == R.id.eb_history_list) showNext();
            // If currently showing result list, toggle.
            if (mResultEmptySwitcher.getCurrentView().getId() == R.id.eb_result_list) {
                mResultEmptySwitcher.showNext();
            }
        }
    }


}
