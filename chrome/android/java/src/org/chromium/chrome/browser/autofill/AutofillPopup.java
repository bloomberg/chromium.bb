// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ListAdapter;
import android.widget.ListPopupWindow;
import android.widget.TextView;

import java.util.ArrayList;

import org.chromium.chrome.R;
import org.chromium.content.browser.ContainerViewDelegate;
import org.chromium.ui.gfx.NativeWindow;

/**
 * The Autofill suggestion popup that lists relevant suggestions.
 */
public class AutofillPopup extends ListPopupWindow implements AdapterView.OnItemClickListener {

    /**
     * Constants defining types of Autofill suggestion entries.
     * Has to be kept in sync with enum in WebAutofillClient.h
     *
     * Not supported: MenuItemIDWarningMessage, MenuItemIDSeparator, MenuItemIDClearForm, and
     * MenuItemIDAutofillOptions.
     */
    private static final int ITEM_ID_AUTOCOMPLETE_ENTRY = 0;
    private static final int ITEM_ID_PASSWORD_ENTRY = -2;
    private static final int ITEM_ID_DATA_LIST_ENTRY = -6;

    private static final int TEXT_PADDING_DP = 30;

    private final AutofillPopupDelegate mAutofillCallback;
    private final NativeWindow mNativeWindow;
    private final ContainerViewDelegate mContainerViewDelegate;
    private AnchorView mAnchorView;
    private Rect mAnchorRect;
    private Paint mNameViewPaint;
    private Paint mLabelViewPaint;

    /**
     * An interface to handle the touch interaction with an AutofillPopup object.
     */
    public interface AutofillPopupDelegate {
        /**
         * Handles the dismissal of the AutofillPopup.
         */
        public void dismiss();

        /**
         * Handles the selection of an Autofill suggestion from an AutofillPopup.
         * @param listIndex The index of the selected Autofill suggestion.
         * @param value The value of the selected Autofill suggestion.
         * @param uniqueId The unique id of the selected Autofill suggestion.
         */
        public void suggestionSelected(int listIndex, String value, int uniqueId);
    }

    // ListPopupWindow needs an anchor view to determine it's size and position.  We create a view
    // with the given desired width at the text edit area as a stand-in.  This is "Fake" in the
    // sense that it draws nothing, accepts no input, and thwarts all attempts at laying it out
    // "properly".
    private static class AnchorView extends View {
        private int mCurrentOrientation;
        private AutofillPopup mAutofillPopup;

        AnchorView(Context c, AutofillPopup autofillPopup) {
            super(c);
            mAutofillPopup = autofillPopup;
            mCurrentOrientation = getResources().getConfiguration().orientation;
        }

        @Override
        protected void onConfigurationChanged(Configuration newConfig) {
            super.onConfigurationChanged(newConfig);
            if (newConfig.orientation != mCurrentOrientation) mAutofillPopup.dismiss();
        }

        public void setSize(Rect r, int desiredWidth) {
            setLeft(r.left);
            // If the desired width is smaller than the text edit box, use the width of the text
            // editbox.
            if (desiredWidth < r.right - r.left) {
                setRight(r.right);
            } else {
                // Make sure that the anchor view does not go outside the screen.
                Point size = new Point();
                WindowManager wm = (WindowManager) getContext().getSystemService(
                        Context.WINDOW_SERVICE);
                wm.getDefaultDisplay().getSize(size);
                if (r.left + desiredWidth > size.x) {
                    setRight(size.x);
                } else {
                    setRight(r.left + desiredWidth);
                }
            }
            setBottom(r.bottom);
            setTop(r.top);
        }
    }

    /**
     * Creates an AutofillWindow with specified parameters.
     * @param nativeWindow NativeWindow used to get application context.
     * @param containerViewDelegate View delegate used to add and remove views.
     * @param autofillCallback A object that handles the calls to the native AutofillPopupView.
     */
    public AutofillPopup(NativeWindow nativeWindow, ContainerViewDelegate containerViewDelegate,
            AutofillPopupDelegate autofillCallback) {
        super(nativeWindow.getContext());
        mNativeWindow = nativeWindow;
        mContainerViewDelegate = containerViewDelegate;
        mAutofillCallback = autofillCallback;

        setOnItemClickListener(this);
        setWidth(ListPopupWindow.WRAP_CONTENT);

        mAnchorView = new AnchorView(mNativeWindow.getContext(), this);
        mContainerViewDelegate.addViewToContainerView(mAnchorView);
    }

    /**
     * Sets the Autofill suggestions to display in the popup.
     * @param suggestions Autofill suggestion data.
     */
    public void show(AutofillSuggestion[] suggestions) {
        // Remove the AutofillSuggestions with IDs that are not supported by Android
        ArrayList<AutofillSuggestion> cleanedData = new ArrayList<AutofillSuggestion>();
        for (int i = 0; i < suggestions.length; i++) {
            int itemId = suggestions[i].mUniqueId;
            if (itemId > 0 || itemId == ITEM_ID_AUTOCOMPLETE_ENTRY ||
                    itemId == ITEM_ID_PASSWORD_ENTRY || itemId == ITEM_ID_DATA_LIST_ENTRY) {
                cleanedData.add(suggestions[i]);
            }
        }
        mAnchorView.setSize(mAnchorRect, getDesiredWidth(suggestions));
        setAnchorView(mAnchorView);
        setContentWidth(mAnchorView.getWidth());
        Drawable popupBackground = getBackground();
        if (popupBackground != null) {
            Rect paddingRect = new Rect();
            popupBackground.getPadding(paddingRect);
            setHorizontalOffset(-paddingRect.left);
        }
        setAdapter(new AutofillListAdapter(mNativeWindow.getContext(), cleanedData));
        show();
    }

    /**
     * Overriding standard behavior to let native AutofillPopupView to deal with dismissing.
     */
    @Override
    public void dismiss() {
        mAutofillCallback.dismiss();
    }

    /**
     * Dismisses the popup and removes it from the anchor from the ContainerView.
     */
    public void dismissFromNative() {
        super.dismiss();
        mContainerViewDelegate.removeViewFromContainerView(mAnchorView);
    }

    /**
     * Get desired popup window width by calculating the maximum text length from Autofill data.
     * @param data Autofill suggestion data.
     * @return The popup window width.
     */
    private int getDesiredWidth(AutofillSuggestion[] data) {
        if (mNameViewPaint == null || mLabelViewPaint == null) {
            LayoutInflater inflater =
                    (LayoutInflater) mNativeWindow.getContext().getSystemService(
                            Context.LAYOUT_INFLATER_SERVICE);
            View layout = inflater.inflate(R.layout.autofill_text, null);
            TextView nameView = (TextView) layout.findViewById(R.id.autofill_name);
            mNameViewPaint = nameView.getPaint();
            TextView labelView = (TextView) layout.findViewById(R.id.autofill_label);
            mLabelViewPaint = labelView.getPaint();
        }

        int maxTextWidth = 0;
        Rect bounds = new Rect();
        for (int i = 0; i < data.length; ++i) {
            bounds.setEmpty();
            String name = data[i].mName;
            int width = 0;
            mNameViewPaint.getTextBounds(name, 0, name.length(), bounds);
            width += bounds.width();

            bounds.setEmpty();
            String label = data[i].mLabel;
            mLabelViewPaint.getTextBounds(label, 0, label.length(), bounds);
            width += bounds.width();
            maxTextWidth = Math.max(width, maxTextWidth);
        }
        // Adding padding.
        return maxTextWidth + (int) (TEXT_PADDING_DP *
                mNativeWindow.getContext().getResources().getDisplayMetrics().density);
    }

    /**
     * Sets the location and the size of the anchor view that the AutofillPopup will use to attach
     * itself.
     * @param x X coordinate of the top left corner of the anchor view.
     * @param y Y coordinate of the top left corner of the anchor view.
     * @param width The width of the anchor view.
     * @param height The height of the anchor view.
     */
    public void setAnchorRect(float x, float y, float width, float height) {
        mAnchorRect = new Rect((int) x, (int) y, (int) (x + width), (int) (y + height));
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        try {
            ListAdapter adapter = (ListAdapter) parent.getAdapter();
            AutofillSuggestion data = (AutofillSuggestion) adapter.getItem(position);
            mAutofillCallback.suggestionSelected(position, data.mName, data.mUniqueId);
        } catch (ClassCastException e) {
            Log.w("AutofillWindow", "error in onItemClick", e);
            assert false;
        }
    }

}
