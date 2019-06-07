// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.widget.TintedDrawable;

/**
 * Container view for omnibox entity suggestions.
 */
public class EntitySuggestionView extends RelativeLayout {
    /**
     * EventListener is a class receiving all untranslated input events.
     */
    interface EventListener {
        /**
         * Process gesture event described by supplied MotionEvent.
         * @param event Gesture motion event.
         */
        void onMotionEvent(MotionEvent event);

        /** Process click event. */
        void onClick();

        /** Process long click event. */
        void onLongClick();

        /** Process select/highlight event. */
        void onSelected();

        /** Process refine event. */
        void onRefine();
    }

    private EventListener mEventListener;
    private View mEntityView;
    private TextView mSubjectText;
    private TextView mDescriptionText;
    private ImageView mEntityImageView;
    private ImageView mRefineView;
    private boolean mUseDarkColors;
    private boolean mUseSuggestionImage;

    /**
     * Container view for omnibox suggestions allowing soft focus from keyboard.
     */
    public static class FocusableView extends RelativeLayout {
        /** Creates new instance of FocusableView. */
        public FocusableView(Context context, AttributeSet attributes) {
            super(context, attributes);
        }

        @Override
        public boolean isFocused() {
            return super.isFocused() || (isSelected() && !isInTouchMode());
        }
    }

    /** Creates new instance of EntitySuggestionView. */
    public EntitySuggestionView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSubjectText = findViewById(R.id.omnibox_entity_subject_text);
        mDescriptionText = findViewById(R.id.omnibox_entity_description_text);
        mEntityImageView = findViewById(R.id.omnibox_entity_image);
        mEntityView = findViewById(R.id.omnibox_entity);
        mRefineView = findViewById(R.id.omnibox_entity_refine_icon);

        showSearchIcon();
    }

    @Override
    public void setSelected(boolean selected) {
        super.setSelected(selected);
        mEntityView.setSelected(selected);
        if (mEventListener != null && selected && !isInTouchMode()) {
            mEventListener.onSelected();
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (mEventListener != null) {
            mEventListener.onMotionEvent(ev);
        }
        return super.dispatchTouchEvent(ev);
    }

    /** Specify delegate receiving click/refine events. */
    void setDelegate(EventListener delegate) {
        mEventListener = delegate;
        mEntityView.setOnClickListener((View v) -> mEventListener.onClick());
        mEntityView.setOnLongClickListener((View v) -> {
            mEventListener.onLongClick();
            return true;
        });
        mRefineView.setOnClickListener((View v) -> mEventListener.onRefine());
    }

    /**
     * Toggles theme.
     * @param useDarkColors specifies whether UI should use dark theme.
     */
    void setUseDarkColors(boolean useDarkColors) {
        Drawable drawable = mRefineView.getDrawable();
        DrawableCompat.setTint(
                drawable, ColorUtils.getIconTint(getContext(), !useDarkColors).getDefaultColor());
        mUseDarkColors = useDarkColors;
        if (!mUseSuggestionImage) {
            showSearchIcon();
        }
    }

    /**
     * Specifies the text to be displayed as subject name.
     * @param text Text to be displayed.
     */
    void setSubjectText(String text) {
        mSubjectText.setText(text);
    }

    /**
     * Specifies the text to be displayed as description.
     * @param text Text to be displayed.
     */
    void setDescriptionText(String text) {
        mDescriptionText.setText(text);
    }

    /**
     * Specify image to be shown beside suggestion text.
     * @param drawable Image to be rendered.
     */
    void setSuggestionImage(Drawable drawable) {
        mUseSuggestionImage = true;
        mEntityImageView.setImageDrawable(drawable);
        mEntityImageView.setScaleType(ImageView.ScaleType.FIT_CENTER);
    }

    /**
     * Clear suggestion image and present (fallback) magnifying glass instead.
     */
    void clearSuggestionImage() {
        mUseSuggestionImage = false;
        showSearchIcon();
    }

    private void showSearchIcon() {
        mEntityImageView.setImageDrawable(TintedDrawable.constructTintedDrawable(getContext(),
                R.drawable.ic_suggestion_magnifier,
                mUseDarkColors ? R.color.default_icon_color_secondary_list
                               : R.color.white_mode_tint));
        mEntityImageView.setScaleType(ImageView.ScaleType.CENTER);
    }
}
