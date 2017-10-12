// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.app.Activity;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v7.widget.SwitchCompat;
import android.view.View;
import android.widget.CompoundButton;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AccessibilityUtil;
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

    /** Whether or not the user made a selection by tapping the 'ok' button. */
    private boolean mUserMadeSelection;

    /**
     * Default constructor.
     * @param activity The {@link Activity} showing the promo.
     */
    public ChromeHomePromoDialog(Activity activity) {
        super(activity);
        setOnDismissListener(this);
    }

    @Override
    protected DialogParams getDialogParams() {
        PromoDialog.DialogParams params = new PromoDialog.DialogParams();
        params.headerStringResource = R.string.chrome_home_promo_dialog_title;
        params.subheaderStringResource = AccessibilityUtil.isAccessibilityEnabled()
                ? R.string.chrome_home_promo_dialog_message_accessibility
                : R.string.chrome_home_promo_dialog_message;
        params.primaryButtonStringResource = R.string.ok;
        if (SysUtils.isLowEndDevice()) {
            params.drawableResource = R.drawable.chrome_home_promo_static;
        } else {
            params.drawableInstance = new ChromeHomePromoIllustration(getContext());
        }

        return params;
    }

    @Override
    public void onClick(View view) {
        mUserMadeSelection = true;

        // There is only one button for this dialog, so dismiss on any click.
        dismiss();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View toggleLayout = getLayoutInflater().inflate(R.layout.chrome_home_promo_toggle, null);

        SwitchCompat toggle = (SwitchCompat) toggleLayout.findViewById(R.id.chrome_home_toggle);
        toggle.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean enabled) {
                mSwitchStateShouldEnable = enabled;
            }
        });

        toggle.setChecked(FeatureUtilities.isChromeHomeEnabled());
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

        ChromeActivity activity = (ChromeActivity) getOwnerActivity();
        Tab tab = activity.getActivityTab();

        boolean showOptOutSnackbar = false;
        if (!mSwitchStateShouldEnable
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_HOME_OPT_OUT_SNACKBAR)) {
            try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                showOptOutSnackbar =
                        !ChromePreferenceManager.getInstance().getChromeHomeOptOutSnackbarShown();
            }
        }

        Runnable finalizeCallback = null;
        if (showOptOutSnackbar) {
            finalizeCallback = new Runnable() {
                @Override
                public void run() {
                    ChromeHomeSnackbarController.initialize(tab);
                }
            };
        }

        // Detach the foreground tab and. It will be reattached when the activity is restarted.
        tab.detachAndStartReparenting(null, null, finalizeCallback);

        getOwnerActivity().recreate();
    }

    @Override
    public void onDismiss(DialogInterface dialogInterface) {
        // If the user did not hit 'ok', do not use the switch value to store the user setting.
        boolean userSetting = mUserMadeSelection ? mSwitchStateShouldEnable
                                                 : FeatureUtilities.isChromeHomeEnabled();

        boolean restartRequired = userSetting != FeatureUtilities.isChromeHomeEnabled();
        FeatureUtilities.switchChromeHomeUserSetting(userSetting);

        if (restartRequired) restartChromeInstances();
    }
}
