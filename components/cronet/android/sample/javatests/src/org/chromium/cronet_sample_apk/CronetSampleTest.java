// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.ConditionVariable;
import android.test.ActivityInstrumentationTestCase2;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.TextView;

import org.chromium.base.test.util.Feature;

/**
 * Base test class for all CronetSample based tests.
 */
public class CronetSampleTest extends
        ActivityInstrumentationTestCase2<CronetSampleActivity> {

    // URL used for base tests.
    private static final String URL = "http://127.0.0.1:8000";

    public CronetSampleTest() {
        super(CronetSampleActivity.class);
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetSampleActivity activity = launchCronetSampleWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        // Verify successful fetch.
        final TextView textView = (TextView) activity.findViewById(R.id.resultView);
        final ConditionVariable done = new ConditionVariable();
        final TextWatcher textWatcher = new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {}

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (s.equals("Completed " + URL + " (200)")) {
                    done.open();
                }
            }
        };
        textView.addTextChangedListener(textWatcher);
        // Check current text in case it changed before |textWatcher| was added.
        textWatcher.onTextChanged(textView.getText(), 0, 0, 0);
        done.block();
    }

    /**
     * Starts the CronetSample activity and loads the given URL.
     */
    protected CronetSampleActivity launchCronetSampleWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(
                getInstrumentation().getTargetContext(),
                CronetSampleActivity.class));
        setActivityIntent(intent);
        return getActivity();
    }
}
