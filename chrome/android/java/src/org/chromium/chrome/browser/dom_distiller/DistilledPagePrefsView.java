// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.AlertDialog;
import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.Theme;

import java.util.EnumMap;
import java.util.Map;

/**
 * A view which displays preferences for distilled pages.  This allows users
 * to change the theme, font size, etc. of distilled pages.
 */
public class DistilledPagePrefsView extends RadioGroup
        implements DistilledPagePrefs.Observer {
    // XML layout for View.
    private static final int VIEW_LAYOUT = R.layout.distilled_page_prefs_view;

    // Buttons for color mode.
    private final Map<Theme, RadioButton> mColorModeButtons;

    private final DistilledPagePrefs mDistilledPagePrefs;

    /**
     * Creates a DistilledPagePrefsView.
     *
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public DistilledPagePrefsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDistilledPagePrefs = DomDistillerServiceFactory.getForProfile(
                Profile.getLastUsedProfile()).getDistilledPagePrefs();
        mColorModeButtons = new EnumMap<Theme, RadioButton>(Theme.class);
    }

    public static DistilledPagePrefsView create(Context context) {
        return (DistilledPagePrefsView) LayoutInflater.from(context)
                .inflate(VIEW_LAYOUT, null);
    }

    public static void showDialog(Context context) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setView(DistilledPagePrefsView.create(context));
        builder.show();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mColorModeButtons.put(Theme.LIGHT,
                initializeAndGetButton(R.id.light_mode, Theme.LIGHT));
        mColorModeButtons.put(Theme.DARK,
                initializeAndGetButton(R.id.dark_mode, Theme.DARK));
        mColorModeButtons.put(Theme.SEPIA,
                initializeAndGetButton(R.id.sepia_mode, Theme.SEPIA));
        mColorModeButtons.get(mDistilledPagePrefs.getTheme()).setChecked(true);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setOrientation(HORIZONTAL);

        for (RadioButton button : mColorModeButtons.values()) {
            ViewGroup.LayoutParams layoutParams =
                    (ViewGroup.LayoutParams) button.getLayoutParams();
            layoutParams.width = 0;
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // If text is wider than button, change layout so that buttons are stacked on
        // top of each other.
        for (RadioButton button : mColorModeButtons.values()) {
            if (button.getLineCount() > 1) {
                setOrientation(VERTICAL);
                for (RadioButton innerLoopButton : mColorModeButtons.values()) {
                    ViewGroup.LayoutParams layoutParams =
                            (ViewGroup.LayoutParams) innerLoopButton.getLayoutParams();
                    layoutParams.width = LayoutParams.MATCH_PARENT;
                }
                break;
            }
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mDistilledPagePrefs.addObserver(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mDistilledPagePrefs.removeObserver(this);
    }

    // DistilledPagePrefs.Observer

    /**
     * Changes which button is selected if the theme is changed in another tab.
     */
    @Override
    public void onChangeTheme(Theme theme) {
        mColorModeButtons.get(theme).setChecked(true);
    }

    /**
     * Initiatializes a Button and selects it if it corresponds to the current
     * theme.
     */
    private RadioButton initializeAndGetButton(int id, final Theme theme) {
        final RadioButton button = (RadioButton) findViewById(id);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mDistilledPagePrefs.setTheme(theme);
            }
        });
        return button;
    }
}
