// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;
import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog prompting a user to either enable integration with Digital Wellbeing or to revoke
 * permission for that integration.
 * TODO(pnoland): Revisit the style of this dialog and where it's used(i.e. whether it's used from
 * PrivacyPreferences or not) to ensure that the visual style is consistent.
 */
public class UsageStatsConsentDialog {
    private Activity mActivity;
    private ModalDialogManager mManager;
    private PropertyModel mDialogModel;

    private boolean mIsRevocation;
    private boolean mIsStandaloneActivity;

    public static UsageStatsConsentDialog create(
            Activity activity, boolean isRevocation, boolean isStandaloneActivity) {
        return new UsageStatsConsentDialog(activity, isRevocation, isStandaloneActivity);
    }

    /** Show this dialog in the context of its enclosing activity. */
    public void show() {
        Resources resources = mActivity.getResources();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, makeController())
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel);
        if (mIsRevocation) {
            builder.with(ModalDialogProperties.TITLE, resources,
                           R.string.usage_stats_revocation_prompt)
                    .with(ModalDialogProperties.MESSAGE, resources,
                            R.string.usage_stats_revocation_explanation)
                    .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.remove);
        } else {
            builder.with(ModalDialogProperties.TITLE, resources, R.string.usage_stats_consent_title)
                    .with(ModalDialogProperties.MESSAGE, resources,
                            R.string.usage_stats_consent_prompt)
                    .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.show);
        }

        mDialogModel = builder.build();
        mManager = new ModalDialogManager(new AppModalPresenter(mActivity), ModalDialogType.APP);
        mManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private UsageStatsConsentDialog(
            Activity activity, boolean isRevocation, boolean isStandaloneActivity) {
        mActivity = activity;
        mIsRevocation = isRevocation;
        mIsStandaloneActivity = isStandaloneActivity;
    }

    private ModalDialogProperties.Controller makeController() {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                UsageStatsService service = UsageStatsService.getInstance();
                int result = Activity.RESULT_CANCELED;
                switch (buttonType) {
                    case ModalDialogProperties.ButtonType.POSITIVE:
                        service.setOptInState(!mIsRevocation);
                        result = Activity.RESULT_OK;
                        break;
                    case ModalDialogProperties.ButtonType.NEGATIVE:
                        service.setOptInState(mIsRevocation);
                        result = Activity.RESULT_CANCELED;
                        break;
                }

                if (mIsStandaloneActivity) {
                    mActivity.setResult(result);
                }

                dismiss();
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (mIsStandaloneActivity) {
                    mActivity.finish();
                }

                mManager.destroy();
            }
        };
    }

    private void dismiss() {
        mManager.destroy();
    }
}