// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.policy.PolicyProvider;

/** Instrumentation tests for {@link CombinedPolicyProvider} */
public class CombinedPolicyProviderTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String DATA_URI = "data:text/plain;charset=utf-8;base64,dGVzdA==";

    public CombinedPolicyProviderTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Checks that the {@link CombinedPolicyProvider} properly notifies tabs when incognito mode is
     * disabled.
     */
    @Feature({"Policy" })
    @SmallTest
    public void testTerminateIncognitoSon() throws InterruptedException {
        final boolean incognitoMode = true;

        TabModel incognitoTabModel = getTabModel(incognitoMode);
        loadUrlInNewTab(DATA_URI, incognitoMode);
        loadUrlInNewTab(DATA_URI, incognitoMode);
        assertEquals(2, incognitoTabModel.getCount());

        final CombinedPolicyProvider provider = CombinedPolicyProvider.get();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                provider.registerProvider(new PolicyProvider() {
                    @Override
                    public void refresh() {
                        terminateIncognitoSession();
                    }
                });
            }
        });

        assertEquals(0, incognitoTabModel.getCount());
    }

    /**
     * Returns the appropriate {@link TabModel} depending on whether Chrome is running in Document
     * or Tabbed mode.
     */
    private TabModel getTabModel(boolean incognito) {
        if (FeatureUtilities.isDocumentMode(getInstrumentation().getContext())) {
            return ChromeApplication.getDocumentTabModelSelector().getModel(incognito);
        }
        return getActivity().getTabModelSelector().getModel(incognito);
    }
}
