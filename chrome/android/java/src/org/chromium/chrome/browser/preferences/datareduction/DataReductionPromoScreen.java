// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.datareduction;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.Toast;

/**
 * The promo screen encouraging users to enable Data Saver.
 */
public class DataReductionPromoScreen extends Dialog implements View.OnClickListener,
        DialogInterface.OnDismissListener {
    public static final String FROM_PROMO = "FromPromo";
    private static final String SHARED_PREF_DISPLAYED_PROMO = "displayed_data_reduction_promo";

    private int mState;

    private static View getContentView(Context context) {
        LayoutInflater inflater = (LayoutInflater) context.getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        return inflater.inflate(R.layout.data_reduction_promo_screen, null);
    }

    /**
     * Launch the data reduction promo, if it needs to be displayed.
     */
    public static void launchDataReductionPromo(Activity parentActivity) {
        // The promo is displayed if Chrome is launched directly (i.e., not with the intent to
        // navigate to and view a URL on startup), the instance is part of the field trial,
        // and the promo has not been displayed before.
        if (!DataReductionProxySettings.getInstance().isDataReductionProxyPromoAllowed()) {
            return;
        }
        if (DataReductionProxySettings.getInstance().isDataReductionProxyManaged()) return;
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) return;
        if (getDisplayedDataReductionPromo(parentActivity)) return;
        // Showing the promo dialog in multiwindow mode is broken on Galaxy Note devices:
        // http://crbug.com/354696. If we're in multiwindow mode, save the dialog for later.
        if (MultiWindowUtils.getInstance().isMultiWindow(parentActivity)) return;

        DataReductionPromoScreen promoScreen = new DataReductionPromoScreen(parentActivity);
        promoScreen.setOnDismissListener(promoScreen);
        promoScreen.show();
    }

    /**
     * DataReductionPromoScreen constructor.
     *
     * @param context An Android context.
     */
    public DataReductionPromoScreen(Context context) {
        super(context, R.style.DataReductionPromoScreenDialog);
        setContentView(getContentView(context), new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Keep the window full screen otherwise the flip animation will frame-skip.
        getWindow().setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

        addListenerOnButton();
        addClickableSpan();

        mState = DataReductionProxyUma.ACTION_DISMISSED;
    }

    private void addListenerOnButton() {
        int [] interactiveViewIds = new int[] {
            R.id.no_thanks_button,
            R.id.enable_button_front,
            R.id.close_button_front
        };

        for (int interactiveViewId : interactiveViewIds) {
            findViewById(interactiveViewId).setOnClickListener(this);
        }
    }

    private void addClickableSpan() {
        TextView settings_link = (TextView) findViewById(R.id.data_reduction_title_2_link);
        ClickableSpan clickableSettingsSpan = new ClickableSpan() {
            @Override
            public void onClick(View view) {
                Context context = getContext();
                if (context == null) return;
                Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                        context, DataReductionPreferences.class.getName());
                intent.putExtra(FROM_PROMO, true);
                context.startActivity(intent);
                // Don't report an action. One will be reported in the settings
                // menu saying if the user proceeded to enable the proxy or not
                // after viewing the promo.
                mState = DataReductionProxyUma.ACTION_INDEX_BOUNDARY;
                dismiss();
            }

            @Override
            public void updateDrawState(TextPaint textPaint) {
                textPaint.setColor(textPaint.linkColor);
                textPaint.setUnderlineText(false);
            }
        };
        settings_link.setMovementMethod(LinkMovementMethod.getInstance());
        settings_link.setText(SpanApplier.applySpans(
                getContext().getString(R.string.data_reduction_title_2),
                new SpanInfo("<link>", "</link>", clickableSettingsSpan)));
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();

        if (id == R.id.no_thanks_button) {
            handleCloseButtonPressed();
        } else if (id == R.id.enable_button_front) {
            mState = DataReductionProxyUma.ACTION_ENABLED;
            handleEnableButtonPressed();
        } else if (id == R.id.close_button_front) {
            handleCloseButtonPressed();
        } else {
            assert false : "Unhandled onClick event";
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        setDisplayedDataReductionPromo(getContext(), true);
    }

    private void handleEnableButtonPressed() {
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getContext(), true);
        dismiss();
        Toast.makeText(getContext(), getContext().getString(R.string.data_reduction_enabled_toast),
                Toast.LENGTH_LONG).show();
    }

    private void handleCloseButtonPressed() {
        dismiss();
    }

    @Override
    public void dismiss() {
        if (mState < DataReductionProxyUma.ACTION_INDEX_BOUNDARY) {
            DataReductionProxyUma.dataReductionProxyUIAction(mState);
            mState = DataReductionProxyUma.ACTION_INDEX_BOUNDARY;
        }
        super.dismiss();
    }

    private static boolean getDisplayedDataReductionPromo(Context context) {
        return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
                SHARED_PREF_DISPLAYED_PROMO, false);
    }

    private static void setDisplayedDataReductionPromo(Context context, boolean displayed) {
        PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(SHARED_PREF_DISPLAYED_PROMO, displayed)
                .apply();
    }
}
