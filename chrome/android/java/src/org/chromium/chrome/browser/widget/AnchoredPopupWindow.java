// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * UI component that handles showing a {@link PopupWindow}. Positioning this popup happens through
 * a {@link RectProvider} provided during construction.
 */
public class AnchoredPopupWindow implements OnTouchListener, RectProvider.Observer {
    /**
     * An observer that is notified of AnchoredPopupWindow layout changes.
     */
    public interface LayoutObserver {
        /**
         * Called immediately before the popup layout changes.
         * @param positionBelow Whether the popup is positioned below its anchor rect.
         * @param x The x position for the popup.
         * @param y The y position for the popup.
         * @param width The width of the popup.
         * @param height The height of the popup.
         * @param anchorRect The {@link Rect} used to anchor the popup.
         */
        void onPreLayoutChange(
                boolean positionBelow, int x, int y, int width, int height, Rect anchorRect);
    }

    /** Orientation preferences for the popup */
    public enum Orientation { MAX_AVAILABLE_SPACE, BELOW, ABOVE }

    // Cache Rect objects for querying View and Screen coordinate APIs.
    private final Rect mCachedPaddingRect = new Rect();
    private final Rect mCachedWindowRect = new Rect();

    private final Context mContext;
    private final Handler mHandler;
    private final View mRootView;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final PopupWindow mPopupWindow;

    /** Provides the @link Rect} to anchor the popup to in screen space. */
    private final RectProvider mRectProvider;

    private final Runnable mDismissRunnable = new Runnable() {
        @Override
        public void run() {
            if (mPopupWindow.isShowing()) dismiss();
        }
    };

    private final OnDismissListener mDismissListener = new OnDismissListener() {
        @Override
        public void onDismiss() {
            if (mIgnoreDismissal) return;

            mHandler.removeCallbacks(mDismissRunnable);
            for (OnDismissListener listener : mDismissListeners) listener.onDismiss();

            mRectProvider.stopObserving();
        }
    };

    private boolean mDismissOnTouchInteraction;

    // Pass through for the internal PopupWindow.  This class needs to intercept these for API
    // purposes, but they are still useful to callers.
    private ObserverList<OnDismissListener> mDismissListeners = new ObserverList<>();
    private OnTouchListener mTouchListener;
    private LayoutObserver mLayoutObserver;

    // Positioning/sizing coordinates for the popup.
    private int mX;
    private int mY;
    private int mWidth;
    private int mHeight;

    /** The margin to add to the popup so it doesn't bump against the edges of the screen. */
    private int mMarginPx;

    // Preferred orientation for the popup with respect to the anchor.
    private Orientation mPreferredOrientation = Orientation.MAX_AVAILABLE_SPACE;

    /**
     * Tracks whether or not we are in the process of updating the popup, which might include a
     * dismiss and show.  In that case we don't want to let the world know we're dismissing because
     * it's only temporary.
     */
    private boolean mIgnoreDismissal;

    private boolean mPositionBelow;

    /**
     * Constructs an {@link AnchoredPopupWindow} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param background The background {@link Drawable} to use for the popup.
     * @param contentView The content view to set on the popup.
     * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect} this
     *                           popup attaches and orients to.
     */
    public AnchoredPopupWindow(Context context, View rootView, Drawable background,
            View contentView, RectProvider anchorRectProvider) {
        mContext = context;
        mRootView = rootView.getRootView();
        mPopupWindow = new PopupWindow(mContext);
        mHandler = new Handler();
        mRectProvider = anchorRectProvider;

        mPopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setBackgroundDrawable(background);
        mPopupWindow.setContentView(contentView);

        mPopupWindow.setTouchInterceptor(this);
        mPopupWindow.setOnDismissListener(mDismissListener);
    }

    /** Shows the popup. Will have no effect if the popup is already showing. */
    public void show() {
        if (mPopupWindow.isShowing()) return;

        mRectProvider.startObserving(this);

        updatePopupLayout();
        mPopupWindow.showAtLocation(mRootView, Gravity.TOP | Gravity.START, mX, mY);
    }

    /**
     * Disposes of the popup window.  Will have no effect if the popup isn't showing.
     * @see PopupWindow#dismiss()
     */
    public void dismiss() {
        mPopupWindow.dismiss();
    }

    /**
     * @return Whether the popup is currently showing.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /**
     * Sets the {@link LayoutObserver} for this AnchoredPopupWindow.
     * @param layoutObserver The observer to be notified of layout changes.
     */
    public void setLayoutObserver(LayoutObserver layoutObserver) {
        mLayoutObserver = layoutObserver;
    }

    /**
     * @param onTouchListener A callback for all touch events being dispatched to the popup.
     * @see PopupWindow#setTouchInterceptor(OnTouchListener)
     */
    public void setTouchInterceptor(OnTouchListener onTouchListener) {
        mTouchListener = onTouchListener;
    }

    /**
     * @param onDismissListener A listener to be called when the popup is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void addOnDismissListener(OnDismissListener onDismissListener) {
        mDismissListeners.addObserver(onDismissListener);
    }

    /**
     * @param onDismissListener The listener to remove and not call when the popup is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void removeOnDismissListener(OnDismissListener onDismissListener) {
        mDismissListeners.removeObserver(onDismissListener);
    }

    /**
     * @param dismiss Whether or not to dismiss this popup when the screen is tapped.  This will
     *                happen for both taps inside and outside the popup.  The default is
     *                {@code false}.
     */
    public void setDismissOnTouchInteraction(boolean dismiss) {
        mDismissOnTouchInteraction = dismiss;
        mPopupWindow.setOutsideTouchable(mDismissOnTouchInteraction);
    }

    /**
     * Sets the preferred orientation of the popup with respect to the anchor view such as above or
     * below the anchor.
     * @param orientation The orientation preferred.
     */
    public void setPreferredOrientation(Orientation orientation) {
        mPreferredOrientation = orientation;
    }

    /**
     * Sets the animation style for the popup.
     * @param animationStyleId The id of the animation style.
     */
    public void setAnimationStyle(int animationStyleId) {
        mPopupWindow.setAnimationStyle(animationStyleId);
    }

    /**
     * Sets the margin for the popup window.
     * @param margin The margin in pixels.
     */
    public void setMargin(int margin) {
        mMarginPx = margin;
    }

    // RectProvider.Observer implementation.
    @Override
    public void onRectChanged() {
        updatePopupLayout();
    }

    @Override
    public void onRectHidden() {
        dismiss();
    }

    /**
     * Causes this popup to position/size itself.  The calculations will happen even if the popup
     * isn't visible.
     */
    private void updatePopupLayout() {
        // Determine the size of the text popup.
        boolean currentPositionBelow = mPositionBelow;
        boolean preferCurrentOrientation = mPopupWindow.isShowing();

        mPopupWindow.getBackground().getPadding(mCachedPaddingRect);
        int paddingX = mCachedPaddingRect.left + mCachedPaddingRect.right;
        int paddingY = mCachedPaddingRect.top + mCachedPaddingRect.bottom;

        int maxContentWidth = mRootView.getWidth() - paddingX - mMarginPx * 2;

        // Determine whether or not the popup should be above or below the anchor.
        // Aggressively try to put it below the anchor.  Put it above only if it would fit better.
        View contentView = mPopupWindow.getContentView();
        int widthSpec = MeasureSpec.makeMeasureSpec(maxContentWidth, MeasureSpec.AT_MOST);
        contentView.measure(widthSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        int idealHeight = contentView.getMeasuredHeight();

        mRootView.getWindowVisibleDisplayFrame(mCachedWindowRect);

        // In multi-window, the coordinates of root view will be different than (0,0).
        // So we translate the coordinates of |mCachedWindowRect| w.r.t. its window.
        int[] rootCoordinates = new int[2];
        mRootView.getLocationOnScreen(rootCoordinates);
        mCachedWindowRect.offset(-rootCoordinates[0], -rootCoordinates[1]);

        Rect anchorRect = mRectProvider.getRect();
        // TODO(dtrainor): This follows the previous logic.  But we should look into if we want to
        // use the root view dimensions instead of the window dimensions here so the popup can't
        // bleed onto the decorations.
        int spaceAboveAnchor = anchorRect.top - mCachedWindowRect.top - paddingY - mMarginPx;
        int spaceBelowAnchor = mCachedWindowRect.bottom - anchorRect.bottom - paddingY - mMarginPx;

        // Bias based on the center of the popup and where it is on the screen.
        boolean idealFitsBelow = idealHeight <= spaceBelowAnchor;
        boolean idealFitsAbove = idealHeight <= spaceAboveAnchor;

        // Position the popup in the largest available space where it can fit.  This will bias the
        // popups to show below the anchor if it will not fit in either place.
        mPositionBelow =
                (idealFitsBelow && spaceBelowAnchor >= spaceAboveAnchor) || !idealFitsAbove;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && currentPositionBelow != mPositionBelow) {
            if (currentPositionBelow && idealFitsBelow) mPositionBelow = true;
            if (!currentPositionBelow && idealFitsAbove) mPositionBelow = false;
        }

        if (mPreferredOrientation == Orientation.BELOW && idealFitsBelow) mPositionBelow = true;
        if (mPreferredOrientation == Orientation.ABOVE && idealFitsAbove) mPositionBelow = false;

        int maxContentHeight = mPositionBelow ? spaceBelowAnchor : spaceAboveAnchor;
        contentView.measure(
                widthSpec, MeasureSpec.makeMeasureSpec(maxContentHeight, MeasureSpec.AT_MOST));

        mWidth = contentView.getMeasuredWidth() + paddingX;
        mHeight = contentView.getMeasuredHeight() + paddingY;

        // Determine the position of the text popup and arrow.
        if (mPositionBelow) {
            mY = anchorRect.bottom;
        } else {
            mY = anchorRect.top - mHeight;
        }

        mX = anchorRect.left + (anchorRect.width() - mWidth) / 2 + mMarginPx;

        // In landscape mode, root view includes the decorations in some devices. So we guard the
        // window dimensions against |mCachedWindowRect.right| instead.
        mX = MathUtils.clamp(mX, mMarginPx, mCachedWindowRect.right - mWidth - mMarginPx);

        if (mLayoutObserver != null) {
            mLayoutObserver.onPreLayoutChange(mPositionBelow, mX, mY, mWidth, mHeight, anchorRect);
        }

        if (mPopupWindow.isShowing() && mPositionBelow != currentPositionBelow) {
            // If the position of the popup has changed, callers may change the background drawable
            // in response. In this case the padding of the background drawable in the PopupWindow
            // changes.
            try {
                mIgnoreDismissal = true;
                mPopupWindow.dismiss();
                mPopupWindow.showAtLocation(mRootView, Gravity.TOP | Gravity.START, mX, mY);
            } finally {
                mIgnoreDismissal = false;
            }
        }

        mPopupWindow.update(mX, mY, mWidth, mHeight);
    }

    // OnTouchListener implementation.
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        boolean returnValue = mTouchListener != null && mTouchListener.onTouch(v, event);
        if (mDismissOnTouchInteraction) dismiss();
        return returnValue;
    }
}
