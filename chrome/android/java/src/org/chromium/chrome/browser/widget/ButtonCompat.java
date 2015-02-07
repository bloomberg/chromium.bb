// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.AnimatorInflater;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.widget.Button;

import org.chromium.chrome.R;

/**
 * A Material-styled button with a customizable background color.
 *
 * Create a button in Java:
 *
 *   new ButtonCompat(context, Color.RED);
 *
 * Create a button in XML:
 *
 *   <ButtonCompat
 *       android:layout_width="wrap_content"
 *       android:layout_height="wrap_content"
 *       android:text="Click me"
 *       chrome:buttonColor="#f00" />
 *
 * On L devices, this is a true Material button. On earlier devices, the button is similar but lacks
 * ripples and lacks a shadow when pressed.
 */
public class ButtonCompat extends Button {

    /** The amount by which to dim the button background when pressed. */
    private static final float PRE_L_DIM_AMOUNT = 0.85f;

    private int mColor;

    /**
     * Returns a new borderless material-style button.
     */
    public static Button createBorderlessButton(Context context) {
        Context wrapper = new ContextThemeWrapper(context, R.style.ButtonBorderlessCompat);
        return new Button(wrapper, null, 0);
    }

    /**
     * Constructs a button with the given buttonColor as its background.
     */
    public ButtonCompat(Context context, int buttonColor) {
        this(context, buttonColor, null);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ButtonCompat(Context context, AttributeSet attrs) {
        this(context, getColorFromAttributeSet(context, attrs), attrs);
    }

    private ButtonCompat(Context context, int buttonColor, AttributeSet attrs) {
        // To apply the ButtonCompat style to this view, use a ContextThemeWrapper to inject the
        // ButtonCompat style into the current theme, and pass 0 as the defStyleAttr to the super
        // constructor to prevent the default Button style from being applied.
        super(new ContextThemeWrapper(context, R.style.ButtonCompat), attrs, 0);

        getBackground().mutate();
        setButtonColor(buttonColor);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Use the StateListAnimator from the Widget.Material.Button style to animate the
            // elevation when the button is pressed.
            TypedArray a = getContext().obtainStyledAttributes(null,
                    new int[]{android.R.attr.stateListAnimator}, 0,
                    android.R.style.Widget_Material_Button);
            setStateListAnimator(AnimatorInflater.loadStateListAnimator(getContext(),
                    a.getResourceId(0, 0)));
            a.recycle();
        }
    }

    /**
     * Sets the background color of the button.
     */
    public void setButtonColor(int color) {
        if (color == mColor) return;
        mColor = color;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            getBackground().setColorFilter(mColor, PorterDuff.Mode.SRC_IN);
        } else {
            updateButtonBackgroundPreL();
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            updateButtonBackgroundPreL();
        }
    }

    private void updateButtonBackgroundPreL() {
        int color = mColor;
        for (int state : getDrawableState()) {
            if (state == android.R.attr.state_pressed
                    || state == android.R.attr.state_focused
                    || state == android.R.attr.state_selected) {
                color = getActiveColorPreL();
                break;
            }
        }

        GradientDrawable background = (GradientDrawable) getBackground();
        background.setColor(color);
    }

    private int getActiveColorPreL() {
        return Color.rgb(
                Math.round(Color.red(mColor) * PRE_L_DIM_AMOUNT),
                Math.round(Color.green(mColor) * PRE_L_DIM_AMOUNT),
                Math.round(Color.blue(mColor) * PRE_L_DIM_AMOUNT)
        );
    }

    private static int getColorFromAttributeSet(Context context, AttributeSet attrs) {
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ButtonCompat, 0, 0);
        int color = a.getColor(R.styleable.ButtonCompat_buttonColor, Color.WHITE);
        a.recycle();
        return color;
    }
}