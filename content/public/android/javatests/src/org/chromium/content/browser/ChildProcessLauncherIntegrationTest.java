// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildConnectionAllocator;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_shell_apk.ChildProcessLauncherTestUtils;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.util.ArrayList;
import java.util.List;

/**
 * Integration test that starts the full shell and load pages to test ChildProcessLauncher
 * and related code.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ChildProcessLauncherIntegrationTest {
    @Rule
    public final ContentShellActivityTestRule mActivityTestRule =
            new ContentShellActivityTestRule();

    private static class TestChildProcessConnectionFactory
            implements ChildConnectionAllocator.ConnectionFactory {
        private final List<TestChildProcessConnection> mConnections = new ArrayList<>();

        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle) {
            TestChildProcessConnection connection = new TestChildProcessConnection(
                    context, serviceName, bindToCaller, bindAsExternalService, serviceBundle);
            mConnections.add(connection);
            return connection;
        }

        public List<TestChildProcessConnection> getConnections() {
            return mConnections;
        }
    }

    private static class TestChildProcessConnection extends ChildProcessConnection {
        private RuntimeException mRemovedBothInitialAndStrongBinding;

        public TestChildProcessConnection(Context context, ComponentName serviceName,
                boolean bindToCaller, boolean bindAsExternalService,
                Bundle childProcessCommonParameters) {
            super(context, serviceName, bindToCaller, bindAsExternalService,
                    childProcessCommonParameters);
        }

        @Override
        protected void unbind() {
            super.unbind();
            if (mRemovedBothInitialAndStrongBinding == null) {
                mRemovedBothInitialAndStrongBinding = new RuntimeException("unbind");
            }
        }

        @Override
        public void removeInitialBinding() {
            super.removeInitialBinding();
            if (mRemovedBothInitialAndStrongBinding == null && !isStrongBindingBound()) {
                mRemovedBothInitialAndStrongBinding = new RuntimeException("removeInitialBinding");
            }
        }

        @Override
        public void removeStrongBinding() {
            super.removeStrongBinding();
            if (mRemovedBothInitialAndStrongBinding == null && !isInitialBindingBound()) {
                mRemovedBothInitialAndStrongBinding = new RuntimeException("removeStrongBinding");
            }
        }

        public void throwIfDroppedBothInitialAndStrongBinding() {
            if (mRemovedBothInitialAndStrongBinding != null) {
                throw mRemovedBothInitialAndStrongBinding;
            }
        }
    }

    @Test
    @MediumTest
    public void testCrossDomainNavigationDoNotLoseImportance() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncherHelper.setSandboxServicesSettingsForTesting(factory,
                10 /* arbitrary number, only realy need 2 */, null /* use default service name */);

        ContentShellActivity activity =
                mActivityTestRule.launchContentShellWithUrlSync("about:blank");
        NavigationController navigationController =
                mActivityTestRule.getWebContents().getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(activity.getActiveContentViewCore());

        mActivityTestRule.loadUrl(navigationController, testCallbackHelperContainer,
                new LoadUrlParams(UrlUtils.getIsolatedTestFileUrl(
                        "content/test/data/android/geolocation.html")));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(1, connections.size());
                connections.get(0).throwIfDroppedBothInitialAndStrongBinding();
            }
        });

        mActivityTestRule.loadUrl(
                navigationController, testCallbackHelperContainer, new LoadUrlParams("data:foo"));
        ChildProcessLauncherTestUtils.runOnLauncherThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(2, connections.size());
                // connections.get(0).didDropBothInitialAndImportantBindings();
                connections.get(1).throwIfDroppedBothInitialAndStrongBinding();
            }
        });
    }
}
