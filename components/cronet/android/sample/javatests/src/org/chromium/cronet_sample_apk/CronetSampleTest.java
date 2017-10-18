// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.ConditionVariable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.rule.ActivityTestRule;
import android.text.Editable;
import android.text.TextWatcher;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Base test class for all CronetSample based tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CronetSampleTest {
    private EmbeddedTestServer mTestServer;
    private String mUrl;

    @Rule
    public ActivityTestRule<CronetSampleActivity> mActivityTestRule =
            new ActivityTestRule<>(CronetSampleActivity.class, false, false);

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUrl = mTestServer.getURL("/echo?status=200");
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    public void testLoadUrl() throws Exception {
        CronetSampleActivity activity = launchCronetSampleWithUrl(mUrl);

        // Make sure the activity was created as expected.
        Assert.assertNotNull(activity);

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
                if (s.toString().equals("Completed " + mUrl + " (200)")) {
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
                InstrumentationRegistry.getTargetContext(), CronetSampleActivity.class));
        return mActivityTestRule.launchActivity(intent);
    }
}
