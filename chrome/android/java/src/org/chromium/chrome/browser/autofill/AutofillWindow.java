// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListAdapter;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.TextView;

import java.util.ArrayList;

import org.chromium.content.app.AppResource;
import org.chromium.content.browser.ContainerViewDelegate;
import org.chromium.ui.gfx.NativeWindow;

/**
 * The Autofill suggestion window that lists relevant suggestions.
 */
public class AutofillWindow extends ListPopupWindow {

    private static final int TEXT_PADDING_DP = 30;

    private NativeWindow mNativeWindow;
    private int mDesiredWidth;
    private AnchorView mAnchorView;
    private ContainerViewDelegate mContainerViewDelegate;
    private Paint mNameViewPaint;
    private Paint mLabelViewPaint;

    private static class AutofillListAdapter extends ArrayAdapter<AutofillSuggestion> {
        private Context mContext;

        AutofillListAdapter(Context context, ArrayList<AutofillSuggestion> objects) {
            super(context, AppResource.LAYOUT_AUTOFILL_TEXT, objects);
            mContext = context;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View layout = convertView;
            if (convertView == null) {
                LayoutInflater inflater =
                        (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                layout = inflater.inflate(AppResource.LAYOUT_AUTOFILL_TEXT, null);
            }
            TextView nameView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_NAME);
            nameView.setText(getItem(position).mName);
            TextView labelView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_LABEL);
            labelView.setText(getItem(position).mLabel);

            return layout;
        }
    }

    // ListPopupWindow needs an anchor view to determine it's size and position.  We create a view
    // with the given desired width at the text edit area as a stand-in.  This is "Fake" in the
    // sense that it draws nothing, accepts no input, and thwarts all attempts at laying it out
    // "properly".
    private static class AnchorView extends View {
        AnchorView(Context c) {
            super(c);
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
     * @param ContainerViewDelegate View delegate used to add and remove views.
     * @param autofillExternalDelegate The autofill delegate.
     * @param data The autofill suggestions.
     */
    public AutofillWindow(NativeWindow nativeWindow, ContainerViewDelegate ContainerViewDelegate,
            final AutofillExternalDelegate autofillExternalDelegate, AutofillSuggestion[] data) {
        super(nativeWindow.getContext());
        mNativeWindow = nativeWindow;
        mContainerViewDelegate = ContainerViewDelegate;
        setAutofillSuggestions(data);
        setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                try {
                    ListAdapter adapter = (ListAdapter) parent.getAdapter();
                    AutofillSuggestion data = (AutofillSuggestion) adapter.getItem(position);
                    autofillExternalDelegate.autofillPopupSelected(position, data.mName,
                            data.mUniqueId);
                } catch (ClassCastException e) {
                    Log.w("AutofillWindow", "error in onItemClick", e);
                    assert false;
                }
            }
        });
        setWidth(ListPopupWindow.WRAP_CONTENT);
    }

    /**
     * Sets the position of the autofill popup "around" the input rectangle.
     * @param r The rectangle location for the autofill popup.
     */
    public void setPositionAround(Rect r) {
        if (mAnchorView == null) {
            mAnchorView = new AnchorView(mNativeWindow.getContext());
            mContainerViewDelegate.addViewToContainerView(mAnchorView);
            // When the popup goes away we have to remove the fake anchor view.
            setOnDismissListener(new PopupWindow.OnDismissListener() {
                @Override
                public void onDismiss() {
                    mContainerViewDelegate.removeViewFromContainerView(mAnchorView);
                    mAnchorView = null;
                }
            });
        }
        mAnchorView.setSize(r, mDesiredWidth);
        setAnchorView(mAnchorView);
    }

    /**
     * Sets the autofill data to display in the popup.
     * @param data Autofill suggestion data.
     */
    public void setAutofillSuggestions(AutofillSuggestion[] data) {
        // Remove the AutofillSuggestions with IDs that are unsupported by Android
        ArrayList<AutofillSuggestion> cleanedData = new ArrayList<AutofillSuggestion>();
        for (int i = 0; i < data.length; i++) {
            if (data[i].mUniqueId >= 0) cleanedData.add(data[i]);
        }

        mDesiredWidth = getDesiredWidth(data);
        setAdapter(new AutofillListAdapter(mNativeWindow.getContext(), cleanedData));
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
            View layout = inflater.inflate(AppResource.LAYOUT_AUTOFILL_TEXT, null);
            TextView nameView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_NAME);
            mNameViewPaint = nameView.getPaint();
            TextView labelView = (TextView) layout.findViewById(AppResource.ID_AUTOFILL_LABEL);
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
}
