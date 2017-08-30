// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
* Lightweight FirstRunActivity. It shows ToS dialog only.
*/
public class LightweightFirstRunActivity extends FirstRunActivity {
    private Button mOkButton;
    private boolean mNativeInitialized;
    private boolean mTriggerAcceptAfterNativeInit;

    @Override
    public void setContentView() {
        super.setContentView();
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)) {
            completeFirstRunExperience();
        }

        setContentView(LayoutInflater.from(LightweightFirstRunActivity.this)
                               .inflate(R.layout.lightweight_fre_tos, null));

        NoUnderlineClickableSpan clickableTermsSpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                CustomTabActivity.showInfoPage(LightweightFirstRunActivity.this,
                        LocalizationUtils.substituteLocalePlaceholder(
                                getString(R.string.chrome_terms_of_service_url)));
            }
        };
        NoUnderlineClickableSpan clickablePrivacySpan = new NoUnderlineClickableSpan() {
            @Override
            public void onClick(View widget) {
                CustomTabActivity.showInfoPage(LightweightFirstRunActivity.this,
                        LocalizationUtils.substituteLocalePlaceholder(
                                getString(R.string.chrome_privacy_notice_url)));
            }
        };
        ((TextView) findViewById(R.id.lightweight_fre_tos_and_privacy))
                .setText(SpanApplier.applySpans(getString(R.string.lightweight_fre_tos_and_privacy),
                        new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                        new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan)));
        ((TextView) findViewById(R.id.lightweight_fre_tos_and_privacy))
                .setMovementMethod(LinkMovementMethod.getInstance());

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
}
