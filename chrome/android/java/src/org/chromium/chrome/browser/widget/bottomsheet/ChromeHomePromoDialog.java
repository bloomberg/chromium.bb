// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.app.Activity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.Switch;
import android.widget.TextView;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.PromoDialog;

import java.lang.ref.WeakReference;

/**
 * A promotion for Chrome Home (the bottom sheet). This dialog can optionally restart the current
 * activity to bring a user in or out of the feature.
 */
public class ChromeHomePromoDialog extends PromoDialog {
    /** Whether or not the switch in the promo is enabled or disabled. */
    private boolean mSwitchStateShouldEnable;

    /**
     * Default constructor.
     * @param activity The {@link Activity} showing the promo.
     */
    public ChromeHomePromoDialog(Activity activity) {
        super(activity);
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.headerStringResource = R.string.chrome_home_promo_dialog_title;
        params.subheaderStringResource = R.string.chrome_home_promo_dialog_message;
        params.primaryButtonStringResource = R.string.ok;
        params.drawableResource = R.drawable.chrome_home_promo_static;

        return params;
    }

    @Override
    public void onClick(View view) {
        if (mSwitchStateShouldEnable != FeatureUtilities.isChromeHomeEnabled()) {
            FeatureUtilities.switchChromeHomeUserSetting(mSwitchStateShouldEnable);
            restartChromeInstances();
        }

        // There is only one button for this dialog, so dismiss on any click.
        dismiss();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final String onString = getContext().getString(R.string.text_on);
        final String offString = getContext().getString(R.string.text_off);

        View toggleLayout = getLayoutInflater().inflate(R.layout.chrome_home_promo_toggle, null);

        final TextView toggleText =
                (TextView) toggleLayout.findViewById(R.id.chrome_home_promo_state_text);
        Switch toggle = (Switch) toggleLayout.findViewById(R.id.chrome_home_toggle);
        toggle.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean enabled) {
                mSwitchStateShouldEnable = enabled;
                toggleText.setText(mSwitchStateShouldEnable ? onString : offString);
            }
        });

        toggle.setChecked(FeatureUtilities.isChromeHomeEnabled());
        toggleText.setText(FeatureUtilities.isChromeHomeEnabled() ? onString : offString);

        addControl(toggleLayout);
    }

    /**
     * Restart any open Chrome instances, including the activity this promo is running in.
     */
    private void restartChromeInstances() {
        // If there can be multiple activities, restart them before restarting this one.
        if (FeatureUtilities.isTabModelMergingEnabled()) {
            Class<?> otherWindowActivityClass =
                    MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(getOwnerActivity());

            for (WeakReference<Activity> activityRef : ApplicationStatus.getRunningActivities()) {
                Activity activity = activityRef.get();
                if (activity == null) continue;
                if (activity.getClass().equals(otherWindowActivityClass)) {
                    activity.recreate();
                    break;
                }
            }
        }

        getOwnerActivity().recreate();
    }

    @Override
    public void onDismiss(DialogInterface dialogInterface) {}
}
