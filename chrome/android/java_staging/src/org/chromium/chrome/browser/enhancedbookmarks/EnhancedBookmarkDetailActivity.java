// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.transition.Fade;
import android.transition.Transition;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksBridge.SalientImageCallback;
import org.chromium.chrome.browser.enhanced_bookmarks.EnhancedBookmarksModel;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarkDetailScrollView.OnScrollListener;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Dialog to show details of the selected bookmark. It has two modes: read-only mode and editing
 * mode. Clicking on a textview will make the dialog animate to editing mode. On a handset this
 * dialog will be full-screen; on tablet it will be a fixed-size dialog popping up in the middle of
 * the screen.
 */
public class EnhancedBookmarkDetailActivity extends EnhancedBookmarkActivityBase implements
        View.OnClickListener, OnScrollListener {
    public static final String INTENT_BOOKMARK_ID = "EnhancedBookmarkDetailActivity.BookmarkId";
    private static final int ANIMATION_DURATION_MS = 300;

    private EnhancedBookmarksModel mEnhancedBookmarksModel;
    private BookmarkId mBookmarkId;

    private EnhancedBookmarkDetailScrollView mScrollView;
    private LinearLayout mContentLayout;
    private ImageView mImageView;
    private EditText mTitleEditText;
    private EditText mUrlEditText;
    private View mFolderBox;
    private TextView mFolderTextView;
    private EditText mDescriptionEditText;
    private View mMaskView;
    private RelativeLayout mActionBarLayout;
    private ImageButton mCloseButton;
    private ImageButton mDeleteButton;
    private ImageButton mSaveButton;
    private FadingShadowView mImageShadowMask;
    private FadingShadowView mShadow;
    private View mBottomSpacer;
    private EditText[] mEditTexts;

    private Animator mCurrentAnimator;
    private int mDominantColor;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (mBookmarkId.equals(node.getId())) {
                dismiss();
            }
        }

        @Override
        public void bookmarkNodeMoved(BookmarkItem oldParent, int oldIndex, BookmarkItem newParent,
                int newIndex) {
            BookmarkId movedBookmark = mEnhancedBookmarksModel.getChildAt(newParent.getId(),
                    newIndex);
            if (movedBookmark.equals(mBookmarkId)) {
                setParentFolderName(newParent.getId());
            }
        }

        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            if (mBookmarkId.equals(node.getId())) updateViews(node);
        }

        @Override
        public void bookmarkModelChanged() {
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EnhancedBookmarkUtils.setTaskDescriptionInDocumentMode(this,
                getString(R.string.accessibility_enhanced_bookmark_detail_view));
        mEnhancedBookmarksModel = new EnhancedBookmarksModel();
        mBookmarkId = BookmarkId.getBookmarkIdFromString(
                getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        mEnhancedBookmarksModel.addModelObserver(mBookmarkModelObserver);
        setContentView(R.layout.eb_detail);
        mScrollView = (EnhancedBookmarkDetailScrollView) findViewById(
                R.id.eb_detail_scroll_view);
        mContentLayout = (LinearLayout) findViewById(R.id.eb_detail_content);
        mImageView = (ImageView) findViewById(R.id.eb_detail_image_view);
        mTitleEditText = (EditText) findViewById(R.id.eb_detail_title);
        mUrlEditText = (EditText) findViewById(R.id.eb_detail_url);
        mFolderBox = findViewById(R.id.eb_detail_folder_box);
        mFolderTextView = (TextView) findViewById(R.id.eb_detail_folder_textview);
        mDescriptionEditText = (EditText) findViewById(R.id.eb_detail_description);
        mMaskView = findViewById(R.id.eb_detail_image_mask);
        mActionBarLayout = (RelativeLayout) findViewById(R.id.eb_detail_action_bar);
        mSaveButton = (ImageButton) findViewById(R.id.eb_detail_actionbar_save_button);
        mDeleteButton = (ImageButton) findViewById(R.id.eb_detail_actionbar_delete_button);
        mCloseButton = (ImageButton) findViewById(R.id.eb_detail_actionbar_close_button);
        mImageShadowMask = (FadingShadowView) findViewById(R.id.eb_detail_top_shadow);
        mShadow = (FadingShadowView) findViewById(R.id.eb_detail_shadow);
        mBottomSpacer = mScrollView.findViewById(R.id.bottom_spacer);
        mEditTexts = new EditText[]{mTitleEditText, mUrlEditText, mDescriptionEditText};

        // Listen to click event of EditTexts while setting them not focusable in order to
        // postpone showing soft keyboard until animations are finished.
        for (EditText editText : mEditTexts) editText.setOnClickListener(this);
        clearErrorWhenNonEmpty(mEditTexts);
        int shadowColor = getResources().getColor(
                R.color.enhanced_bookmark_detail_dialog_shadow_color);
        mShadow.init(shadowColor, FadingShadow.POSITION_TOP);
        mImageShadowMask.init(shadowColor, FadingShadow.POSITION_TOP);
        mImageShadowMask.setStrength(1.0f);

        mSaveButton.setOnClickListener(this);
        mDeleteButton.setOnClickListener(this);
        mCloseButton.setOnClickListener(this);
        mFolderBox.setOnClickListener(this);

        mScrollView.setOnScrollListener(this);

        updateViews();
        setUpSharedElementAnimation();
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void setUpSharedElementAnimation() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;
        Transition t = new Fade();
        // Exlude status bar and navigation bar, to work around an Android bug that white background
        // activity will cause these two bars blink/flash during content transition.
        t.excludeTarget(android.R.id.statusBarBackground, true);
        t.excludeTarget(android.R.id.navigationBarBackground, true);
        getWindow().setEnterTransition(t);
        getWindow().setExitTransition(t);
    }

    /**
     * Hides soft keyboard and finishes Activity.
     */
    private void dismiss() {
        UiUtils.hideKeyboard(mContentLayout);
        ApiCompatibilityUtils.finishAfterTransition(this);
    }

    @Override
    public void onBackPressed() {
        onClick(mCloseButton);
    }

    private void updateViews() {
        BookmarkItem bookmarkItem = mEnhancedBookmarksModel.getBookmarkById(mBookmarkId);
        updateViews(bookmarkItem);
    }

    /**
     * Updates each EditText to display the data in bookmarkItem. This function will be called every
     * time user cancels editing.
     */
    private void updateViews(BookmarkItem bookmarkItem) {
        mTitleEditText.setText(bookmarkItem.getTitle());
        mUrlEditText.setText(bookmarkItem.getUrl());
        setParentFolderName(bookmarkItem.getParentId());

        if (bookmarkItem.getId().getType() == BookmarkType.PARTNER) {
            mUrlEditText.setEnabled(false);
            mDescriptionEditText.setVisibility(View.GONE);
        } else {
            mUrlEditText.setEnabled(true);
            mDescriptionEditText.setText(
                    mEnhancedBookmarksModel.getBookmarkDescription(mBookmarkId));
            mDescriptionEditText.setVisibility(View.VISIBLE);
        }

        mDominantColor = EnhancedBookmarkUtils.generateBackgroundColor(bookmarkItem);
        mImageView.setBackgroundColor(mDominantColor);
        mMaskView.setBackgroundColor(mDominantColor);
        mEnhancedBookmarksModel.salientImageForUrl(bookmarkItem.getUrl(),
                new SalientImageCallback() {
                    @Override
                    public void onSalientImageReady(Bitmap bitmap, String imageUrl) {
                        if (bitmap == null) return;
                        mImageView.setImageBitmap(bitmap);
                        mDominantColor = EnhancedBookmarkUtils.getDominantColorForBitmap(bitmap);
                        mImageView.setBackgroundColor(mDominantColor);
                        mMaskView.setBackgroundColor(mDominantColor);
                    }
                });
        mMaskView.setAlpha(0.0f);
    }

    private void setParentFolderName(BookmarkId parentId) {
        mFolderTextView.setText(mEnhancedBookmarksModel.getBookmarkTitle(parentId));
        mFolderTextView.setEnabled(parentId.getType() != BookmarkType.PARTNER);
    }

    @Override
    public void onClick(View v) {
        // During animation, do not respond to any clicking events.
        if (mCurrentAnimator != null) return;
        if (v == mCloseButton) {
            dismiss();
        } else if (v == mSaveButton) {
            if (save()) {
                dismiss();
            }
        } else if (v instanceof EditText) {
            scrollToEdit((EditText) v);
        } else if (v == mFolderBox) {
            EnhancedBookmarkFolderSelectActivity.startFolderSelectActivity(this, mBookmarkId);
        } else if (v == mDeleteButton) {
            mEnhancedBookmarksModel.deleteBookmarks(mBookmarkId);
            dismiss();
        }
    }

    /**
     * Editing a TextView will trigger scrolling-up animation, as an effort to smoothly place the
     * focused EditText to be the best editable position to user.
     * <p>
     * To ensure the view can always be scrolled up, before animation:
     *   1) If content is shorter than screen height, then we fill the content view by
     *      emptyAreaHeight so as to align the content and scroll view.
     *   2) Calculate the scrolling amount.
     *   3) If the scrolling amount is larger than the current maximum scrollable amount, increase
     *      height of mBottomSpacer to make the content long enough.
     *   4) trigger scroll-up animation.
     */
    private void scrollToEdit(final EditText editText) {
        if (DeviceFormFactor.isTablet(this)) {
            // On tablet this the size of the dialog is controlled by framework. To avoid any
            // jumpy behavior, we skip the crazy scrolling effect below.
            animateScrolling(mScrollView.getScrollY(), editText);
        } else {
            int scrollAmount = mScrollView.getHeightCompensation() + editText.getTop()
                    - mTitleEditText.getTop();

            // If there is not enough space to scroll, create padding at bottom.
            if (scrollAmount > mScrollView.getMaximumScrollY()) {
                // First try to align content view and scroll view.
                int emptyAreaHeight = mScrollView.getHeight() - mContentLayout.getHeight();
                if (emptyAreaHeight < 0) emptyAreaHeight = 0;
                // Then increase height of bottom spacer to create margin for scroll.
                setViewHeight(mBottomSpacer,
                        scrollAmount - mScrollView.getMaximumScrollY() + emptyAreaHeight);
            }

            animateScrolling(scrollAmount, editText);
        }
    }

    private void animateScrolling(int scrollAmount, final EditText editText) {
        ObjectAnimator animator = ObjectAnimator.ofInt(mScrollView, "scrollY", scrollAmount);
        animator.setDuration(scrollAmount == mScrollView.getScrollY() ? 0 : ANIMATION_DURATION_MS);
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                for (EditText text : mEditTexts) text.setFocusableInTouchMode(true);
                editText.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(editText, 0);
                mCurrentAnimator = null;
            }
        });
        mCurrentAnimator = animator;
        animator.start();
    }

    /**
     * Saves the edited content.
     * @return Whether the content was successfully saved.
     */
    private boolean save() {
        String newTitle = mTitleEditText.getText().toString().trim();
        String newUrl = mUrlEditText.getText().toString().trim();

        boolean urlOrTitleInvalid = false;
        // Fix user input urls, if necessary.
        newUrl = UrlUtilities.fixupUrl(newUrl);
        if (newUrl == null) newUrl = "";
        mUrlEditText.setText(newUrl);
        if (newUrl.isEmpty()) {
            mUrlEditText.setError(getString(R.string.bookmark_missing_url));
            urlOrTitleInvalid = true;
        }
        if (newTitle.isEmpty()) {
            mTitleEditText.setError(getString(R.string.bookmark_missing_title));
            urlOrTitleInvalid = true;
        }
        if (urlOrTitleInvalid) return false;

        BookmarkItem bookmarkItem = mEnhancedBookmarksModel.getBookmarkById(mBookmarkId);
        String newDescription = mDescriptionEditText.getText().toString().trim();
        if (!bookmarkItem.getTitle().equals(newTitle)) {
            mEnhancedBookmarksModel.setBookmarkTitle(mBookmarkId, newTitle);
        }
        if (!bookmarkItem.getUrl().equals(newUrl)
                && bookmarkItem.getId().getType() != BookmarkType.PARTNER) {
            mEnhancedBookmarksModel.setBookmarkUrl(mBookmarkId, newUrl);
        }
        if (bookmarkItem.getId().getType() != BookmarkType.PARTNER) {
            mEnhancedBookmarksModel.setBookmarkDescription(mBookmarkId, newDescription);
        }

        return true;
    }

    @Override
    public void onScrollChanged(int y, int oldY) {
        int offset = mScrollView.getHeightCompensation();
        if (y < offset) {
            mMaskView.setAlpha(y / (float) offset);
            mActionBarLayout.setBackgroundResource(0);
            mShadow.setStrength(0.0f);
        } else {
            mMaskView.setAlpha(1.0f);
            mActionBarLayout.setBackgroundColor(mDominantColor);
            mShadow.setStrength(1.0f);
        }

    }

    /**
     * Adds a listener to EditTexts that clears the TextView's error once the user types something.
     */
    private void clearErrorWhenNonEmpty(TextView... textViews) {
        for (final TextView textView : textViews) {
            textView.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                }
                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {
                }
                @Override
                public void afterTextChanged(Editable s) {
                    if (s.length() != 0 && textView.getError() != null) textView.setError(null);
                }
            });
        }
    }

    @Override
    public void onDestroy() {
        mEnhancedBookmarksModel.removeModelObserver(mBookmarkModelObserver);
        mEnhancedBookmarksModel.destroy();
        mEnhancedBookmarksModel = null;
        super.onDestroy();
    }

    private static void setViewHeight(View view, int height) {
        ViewGroup.LayoutParams lp = view.getLayoutParams();
        lp.height = height;
        view.setLayoutParams(lp);
    }
}
