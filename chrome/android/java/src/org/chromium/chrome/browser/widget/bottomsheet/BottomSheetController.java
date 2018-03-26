// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.view.ViewGroup;

import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.ui.UiUtils;

import java.util.PriorityQueue;

/**
 * This class is responsible for managing the content shown by the {@link BottomSheet}. Features
 * wishing to show content in the {@link BottomSheet} UI must implement {@link BottomSheetContent}
 * and call {@link #requestShowContent(BottomSheetContent, boolean)} which will return true if the
 * content was actually shown (see full doc on method).
 */
public class BottomSheetController {
    /** The initial capacity for the priority queue handling pending content show requests. */
    private static final int INITIAL_QUEUE_CAPACITY = 1;

    /** A handle to the {@link LayoutManager} to determine what state the browser is in. */
    private final LayoutManager mLayoutManager;

    /** A handle to the {@link BottomSheet} that this class controls. */
    private final BottomSheet mBottomSheet;

    /** A queue for content that is waiting to be shown in the {@link BottomSheet}. */
    private PriorityQueue<BottomSheetContent> mContentQueue;

    /** Whether the controller is already processing a hide request for the tab. */
    private boolean mIsProcessingHideRequest;

    /** Track whether the sheet was shown for the current tab. */
    private boolean mWasShownForCurrentTab;

    /**
     * Build a new controller of the bottom sheet.
     * @param tabModelSelector A tab model selector to track events on tabs open in the browser.
     * @param layoutManager A layout manager for detecting changes in the active layout.
     * @param fadingBackgroundView The scrim that shows when the bottom sheet is opened.
     * @param bottomSheet The bottom sheet that this class will be controlling.
     */
    public BottomSheetController(final TabModelSelector tabModelSelector,
            final LayoutManager layoutManager, final FadingBackgroundView fadingBackgroundView,
            BottomSheet bottomSheet) {
        mBottomSheet = bottomSheet;
        mLayoutManager = layoutManager;

        // Watch for navigation and tab switching that close the sheet.
        new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onShown(Tab tab) {
                if (tab != tabModelSelector.getCurrentTab()) return;
                clearRequestsAndHide();
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                if (tab != tabModelSelector.getCurrentTab()) return;
                clearRequestsAndHide();
            }
        };

        // If the layout changes (to tab switcher, toolbar swipe, etc.) hide the sheet.
        mLayoutManager.addSceneChangeObserver(new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {}

            @Override
            public void onSceneChange(Layout layout) {
                // If the tab did not change, reshow the existing content. Once the tab actually
                // changes, existing content and requests will be cleared.
                if (layout instanceof StaticLayout && mWasShownForCurrentTab
                        && mBottomSheet.getCurrentSheetContent() != null) {
                    mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_PEEK, true);
                } else {
                    mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_HIDDEN, false);
                }
            }
        });

        mBottomSheet.addObserver(new EmptyBottomSheetObserver() {
            /**
             * The index of the scrim in the view hierarchy prior to being moved for the bottom
             * sheet.
             */
            private int mOriginalScrimIndexInParent;

            @Override
            public void onTransitionPeekToHalf(float transitionFraction) {
                fadingBackgroundView.setViewAlpha(transitionFraction);
            }

            @Override
            public void onSheetOpened(@BottomSheet.StateChangeReason int reason) {
                mOriginalScrimIndexInParent = UiUtils.getChildIndexInParent(fadingBackgroundView);
                ViewGroup parent = (ViewGroup) fadingBackgroundView.getParent();
                UiUtils.removeViewFromParent(fadingBackgroundView);
                UiUtils.insertBefore(parent, fadingBackgroundView, mBottomSheet);
            }

            @Override
            public void onSheetClosed(@BottomSheet.StateChangeReason int reason) {
                assert mOriginalScrimIndexInParent >= 0;
                ViewGroup parent = (ViewGroup) fadingBackgroundView.getParent();
                UiUtils.removeViewFromParent(fadingBackgroundView);
                parent.addView(fadingBackgroundView, mOriginalScrimIndexInParent);
            }
        });

        // Initialize the queue with a comparator that checks content priority.
        mContentQueue = new PriorityQueue<>(INITIAL_QUEUE_CAPACITY,
                (content1, content2) -> content2.getPriority() - content1.getPriority());
    }

    /**
     * Request that some content be shown in the bottom sheet.
     * @param content The content to be shown in the bottom sheet.
     * @param animate Whether the appearance of the bottom sheet should be animated.
     * @return True if the content was shown, false if it was suppressed. Content is suppressed if
     *         higher priority content is in the sheet, the sheet is expanded beyond the peeking
     *         state, or the browser is in a mode that does not support showing the sheet.
     */
    public boolean requestShowContent(BottomSheetContent content, boolean animate) {
        // If pre-load failed, do nothing. The content will automatically be queued.
        if (!requestContentPreload(content)) return false;
        if (!mBottomSheet.isSheetOpen()) {
            mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_PEEK, animate);
        }
        mWasShownForCurrentTab = true;
        return true;
    }

    /**
     * Request content start loading in the bottom sheet without actually showing the
     * {@link BottomSheet}. Another call to {@link #requestShowContent(BottomSheetContent, boolean)}
     * is required to actually make the sheet appear.
     * @param content The content to pre-load.
     * @return True if the content started loading.
     */
    public boolean requestContentPreload(BottomSheetContent content) {
        if (content == mBottomSheet.getCurrentSheetContent()) return true;
        if (!isValidLayoutShowing()) return false;

        boolean shouldSuppressExistingContent = mBottomSheet.getCurrentSheetContent() != null
                && mBottomSheet.getSheetState() <= BottomSheet.SHEET_STATE_PEEK
                && content.getPriority() < mBottomSheet.getCurrentSheetContent().getPriority()
                && canBottomSheetSwitchContent();

        BottomSheetContent shownContent = mBottomSheet.getCurrentSheetContent();
        if (shouldSuppressExistingContent) {
            mContentQueue.add(mBottomSheet.getCurrentSheetContent());
            shownContent = content;
        } else if (mBottomSheet.getCurrentSheetContent() == null) {
            shownContent = content;
        } else {
            mContentQueue.add(content);
        }

        assert shownContent != null;
        mBottomSheet.showContent(shownContent);

        return shownContent == content;
    }

    /**
     * Hide content shown in the bottom sheet. If the content is not showing, this call retracts the
     * request to show it.
     * @param content The content to be hidden.
     * @param animate Whether the sheet should animate when hiding.
     */
    public void hideContent(BottomSheetContent content, boolean animate) {
        if (content != mBottomSheet.getCurrentSheetContent()) {
            mContentQueue.remove(content);
            return;
        }

        // If the sheet is already processing a request to hide visible content, do nothing.
        if (mIsProcessingHideRequest) return;

        // Handle showing the next content if it exists.
        if (mBottomSheet.getSheetState() == BottomSheet.SHEET_STATE_HIDDEN) {
            // If the sheet is already hidden, simply show the next content.
            showNextContent();
        } else {
            // If the sheet wasn't hidden, wait for it to be before showing the next content.
            BottomSheetObserver hiddenSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int currentState) {
                    // Don't do anything until the sheet is completely hidden.
                    if (currentState != BottomSheet.SHEET_STATE_HIDDEN) return;

                    showNextContent();
                    mBottomSheet.removeObserver(this);
                    mIsProcessingHideRequest = false;
                }
            };

            mIsProcessingHideRequest = true;
            mBottomSheet.addObserver(hiddenSheetObserver);
            mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_HIDDEN, animate);
        }
    }

    /**
     * Show the next {@link BottomSheetContent} if it is available and peek the sheet. If no content
     * is available the sheet's content is set to null.
     */
    private void showNextContent() {
        if (mContentQueue.isEmpty()) {
            mBottomSheet.showContent(null);
            return;
        }

        mBottomSheet.showContent(mContentQueue.poll());
        mBottomSheet.setSheetState(BottomSheet.SHEET_STATE_PEEK, true);
    }

    /**
     * Clear all the content show requests and hide the current content.
     */
    private void clearRequestsAndHide() {
        mContentQueue.clear();
        // TODO(mdjones): Replace usages of bottom sheet with a model in line with MVC.
        // TODO(mdjones): It would probably be useful to expose an observer method that notifies
        //                objects when all content requests are cleared.
        hideContent(mBottomSheet.getCurrentSheetContent(), true);
        mWasShownForCurrentTab = false;
    }

    /**
     * @return Whether the browser is in a layout that supports showing the bottom sheet.
     */
    protected boolean isValidLayoutShowing() {
        return mLayoutManager.getActiveLayout() instanceof StaticLayout;
    }

    /**
     * @return Whether the sheet currently supports switching its content.
     */
    protected boolean canBottomSheetSwitchContent() {
        return mBottomSheet.isSheetOpen();
    }
}
