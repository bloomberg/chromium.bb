// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;
import android.support.test.filters.MediumTest;

import junit.framework.Assert;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.process_launcher.ChildProcessCreationParams;
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

    private static class TestManagedChildProcessConnection extends ManagedChildProcessConnection {
        private RuntimeException mRemovedBothInitialAndStrongBinding;

        public TestManagedChildProcessConnection(Context context,
                BaseChildProcessConnection.DeathCallback deathCallback, String serviceClassName,
                Bundle childProcessCommonParameters, ChildProcessCreationParams creationParams) {
            super(context, deathCallback, serviceClassName, childProcessCommonParameters,
                    creationParams);
        }

        @Override
        public void unbind() {
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
        public void dropOomBindings() {
            super.dropOomBindings();
            if (mRemovedBothInitialAndStrongBinding == null) {
                mRemovedBothInitialAndStrongBinding = new RuntimeException("dropOomBindings");
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

    private static class TestChildProcessConnectionFactory
            implements BaseChildProcessConnection.Factory {
        private final List<TestManagedChildProcessConnection> mConnections = new ArrayList<>();

        @Override
        public BaseChildProcessConnection create(Context context,
                BaseChildProcessConnection.DeathCallback deathCallback, String serviceClassName,
                Bundle childProcessCommonParameters, ChildProcessCreationParams creationParams) {
            TestManagedChildProcessConnection connection =
                    new TestManagedChildProcessConnection(context, deathCallback, serviceClassName,
                            childProcessCommonParameters, creationParams);
            mConnections.add(connection);
            return connection;
        }

        public List<TestManagedChildProcessConnection> getConnections() {
            return mConnections;
        }
    }

    @Test
    @MediumTest
    public void testCrossDomainNavigationDoNotLoseImportance() throws Throwable {
        final TestChildProcessConnectionFactory factory = new TestChildProcessConnectionFactory();
        final List<TestManagedChildProcessConnection> connections = factory.getConnections();
        ChildProcessLauncher.setSandboxServicesSettingsForTesting(factory,
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
