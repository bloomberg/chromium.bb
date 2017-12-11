// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.support.annotation.StringRes;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
* Lightweight FirstRunActivity. It shows ToS dialog only.
*/
public class LightweightFirstRunActivity extends FirstRunActivityBase {
    private FirstRunFlowSequencer mFirstRunFlowSequencer;
    private Button mOkButton;
    private boolean mNativeInitialized;
    private boolean mTriggerAcceptAfterNativeInit;

    public static final String EXTRA_ASSOCIATED_APP_NAME =
            "org.chromium.chrome.browser.firstrun.AssociatedAppName";

    @Override
    public void setContentView() {
        setFinishOnTouchOutside(true);

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                onChildAccountKnown(
                        freProperties.getBoolean(AccountFirstRunFragment.IS_CHILD_ACCOUNT));
            }
        };
        mFirstRunFlowSequencer.start();
    }

    /** Called once it is known whether the device has a child account. */
    public void onChildAccountKnown(boolean hasChildAccount) {
        setContentView(LayoutInflater.from(LightweightFirstRunActivity.this)
                               .inflate(R.layout.lightweight_fre_tos, null));

        NoUnderlineClickableSpan clickableTermsSpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                showInfoPage(R.string.chrome_terms_of_service_url);
            }
        };
        NoUnderlineClickableSpan clickablePrivacySpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                showInfoPage(R.string.chrome_privacy_notice_url);
            }
        };
        NoUnderlineClickableSpan clickableFamilyLinkPrivacySpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                showInfoPage(R.string.family_link_privacy_policy_url);
            }
        };
        String associatedAppName =
                IntentUtils.safeGetStringExtra(getIntent(), EXTRA_ASSOCIATED_APP_NAME);
        if (associatedAppName == null) {
            associatedAppName = "";
        }
        final CharSequence tosAndPrivacyText;
        if (hasChildAccount) {
            tosAndPrivacyText = SpanApplier.applySpans(
                    getString(R.string.lightweight_fre_associated_app_tos_and_privacy_child_account,
                            associatedAppName),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan),
                    new SpanInfo("<LINK3>", "</LINK3>", clickableFamilyLinkPrivacySpan));
        } else {
            tosAndPrivacyText = SpanApplier.applySpans(
                    getString(R.string.lightweight_fre_associated_app_tos_and_privacy,
                            associatedAppName),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan));
        }
        TextView tosAndPrivacyTextView =
                (TextView) findViewById(R.id.lightweight_fre_tos_and_privacy);
        tosAndPrivacyTextView.setText(tosAndPrivacyText);
        tosAndPrivacyTextView.setMovementMethod(LinkMovementMethod.getInstance());

        mOkButton = (Button) findViewById(R.id.lightweight_fre_terms_accept);
        mOkButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                acceptTermsOfService();
            }
        });

        ((Button) findViewById(R.id.lightweight_fre_cancel))
                .setOnClickListener(new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        abortFirstRunExperience();
                    }
                });
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        assert !mNativeInitialized;

        mNativeInitialized = true;
        if (mTriggerAcceptAfterNativeInit) acceptTermsOfService();
    }

    @Override
    public void onBackPressed() {
        abortFirstRunExperience();
    }

    public void abortFirstRunExperience() {
        finish();
        sendPendingIntentIfNecessary(false);
    }

    public void completeFirstRunExperience() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
        finish();

        sendPendingIntentIfNecessary(true);
    }

    private void acceptTermsOfService() {
        if (!mNativeInitialized) {
            mTriggerAcceptAfterNativeInit = true;

            // Disable the "accept" button to indicate that "something is happening".
            mOkButton.setEnabled(false);
            return;
        }
        FirstRunUtils.acceptTermsOfService(false);
        completeFirstRunExperience();
    }

    /**
     * Show an informational web page. The page doesn't show navigation control.
     * @param url Resource id for the URL of the web page.
     */
    public void showInfoPage(@StringRes int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }
}
