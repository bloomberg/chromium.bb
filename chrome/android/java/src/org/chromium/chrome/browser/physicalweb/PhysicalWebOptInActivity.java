// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;

/**
 * This activity invites the user to opt-in to the Physical Web feature.
 */
public class PhysicalWebOptInActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.physical_web_optin);
        PhysicalWebUma.onOptInNotificationPressed(this);

        Button declineButton = (Button) findViewById(R.id.physical_web_decline);
        declineButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                PhysicalWebUma.onOptInDeclineButtonPressed(PhysicalWebOptInActivity.this);
                PrivacyPreferencesManager privacyPrefManager =
                        PrivacyPreferencesManager.getInstance(PhysicalWebOptInActivity.this);
                privacyPrefManager.setPhysicalWebEnabled(false);
                finish();
            }
        });

        Button enableButton = (Button) findViewById(R.id.physical_web_enable);
        enableButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                PhysicalWebUma.onOptInEnableButtonPressed(PhysicalWebOptInActivity.this);
                PrivacyPreferencesManager privacyPrefManager =
                        PrivacyPreferencesManager.getInstance(PhysicalWebOptInActivity.this);
                privacyPrefManager.setPhysicalWebEnabled(true);
                startActivity(createListUrlsIntent(PhysicalWebOptInActivity.this));
                finish();
            }
        });
    }

    private static Intent createListUrlsIntent(Context context) {
        Intent intent = new Intent(context, ListUrlsActivity.class);
        intent.putExtra(ListUrlsActivity.REFERER_KEY,
                ListUrlsActivity.OPTIN_REFERER);
        return intent;
    }
}
