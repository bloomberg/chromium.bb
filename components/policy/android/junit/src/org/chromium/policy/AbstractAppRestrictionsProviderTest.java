// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import java.util.concurrent.Executor;

/**
 * Robolectric test for AbstractAppRestrictionsProvider.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AbstractAppRestrictionsProviderTest {

    /*
     * Robolectric's AsyncTasks don't do anything to override the underlying executor,
     * which means that the don't reliably run within {@link Robolectric.RunBackgroundTasks()}.
     * Create a special task executor that is guarenteed to run when expected.
     */
    private class TestExecutor implements Executor {
        @Override
        public void execute(Runnable command) {
            Robolectric.getBackgroundScheduler().post(command);
        }
    }

    /**
     * Minimal concrete class implementing AbstractAppRestrictionsProvider.
     */
    private class DummyAppRestrictionsProvider extends AbstractAppRestrictionsProvider {

        public DummyAppRestrictionsProvider(Context context) {
            super(context);
        }

        @Override
        protected Bundle getApplicationRestrictions(String packageName) {
            return null;
        }

        @Override
        protected String getRestrictionChangeIntentAction() {
            return null;
        }

    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#refresh()}.
     */
    @Test
    public void testRefresh() {
        // We want to control precisely when background tasks run
        Robolectric.getBackgroundScheduler().pause();

        Context context = Robolectric.application;

        // Clear the preferences
        SharedPreferences sharedPreferences = PreferenceManager
                .getDefaultSharedPreferences(context);
        SharedPreferences.Editor ed = sharedPreferences.edit();
        ed.clear();

        // Set up a bundle for testing.
        Bundle b1 = new Bundle();
        b1.putString("Key1", "value1");
        b1.putInt("Key2", 42);

        // Mock out the histogram functions, since they call statics.
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        provider.setTaskExecutor(new TestExecutor());
        doNothing().when(provider).recordCacheLoadResultHistogram(anyBoolean());
        doNothing().when(provider).recordStartTimeHistogram(anyInt());

        // Set up the buffer to be returned by getApplicationRestrictions.
        when(provider.getApplicationRestrictions(anyString())).thenReturn(b1);

        // Prepare the provider
        CombinedPolicyProvider combinedProvider = mock(CombinedPolicyProvider.class);
        provider.setManagerAndSource(combinedProvider, 0);

        // Refresh with no cache should do nothing immediately.
        provider.refresh();
        verify(combinedProvider, never()).onSettingsAvailable(anyInt(), any(Bundle.class));

        // Let the Async task run and return its result.
        Robolectric.runBackgroundTasks();
        // The AsyncTask should now have got the restrictions.
        verify(provider).getApplicationRestrictions(anyString());
        verify(provider).recordStartTimeHistogram(anyInt());

        Robolectric.runUiThreadTasks();
        // The policies should now have been set.
        verify(combinedProvider).onSettingsAvailable(0, b1);

        // On next refresh onSettingsAvailable should be called twice, once with the current buffer
        // and once with the new data.
        Bundle b2 = new Bundle();
        b2.putString("Key1", "value1");
        b2.putInt("Key2", 84);
        when(provider.getApplicationRestrictions(anyString())).thenReturn(b2);

        provider.refresh();
        verify(combinedProvider, times(2)).onSettingsAvailable(0, b1);
        Robolectric.runBackgroundTasks();
        Robolectric.runUiThreadTasks();
        verify(combinedProvider).onSettingsAvailable(0, b2);
    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#startListeningForPolicyChanges()}.
     */
    @Test
    public void testStartListeningForPolicyChanges() {
        Context context = Robolectric.application;
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        Intent intent = new Intent("org.chromium.test.policy.Hello");
        ShadowApplication shadowApplication = Robolectric.getShadowApplication();

        // If getRestrictionsChangeIntentAction returns null then we should not start a broadcast
        // receiver.
        provider.startListeningForPolicyChanges();
        Assert.assertFalse(shadowApplication.hasReceiverForIntent(intent));

        // If it returns a string then we should.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        Assert.assertTrue(shadowApplication.hasReceiverForIntent(intent));
    }

    /**
     * Test method for {@link AbstractAppRestrictionsProvider#stopListening()}.
     */
    @Test
    public void testStopListening() {
        Context context = Robolectric.application;
        AbstractAppRestrictionsProvider provider = spy(new DummyAppRestrictionsProvider(context));
        Intent intent = new Intent("org.chromium.test.policy.Hello");
        ShadowApplication shadowApplication = Robolectric.getShadowApplication();

        // First try with null result from getRestrictionsChangeIntentAction, only test here is no
        // crash.
        provider.stopListening();

        // Now try starting and stopping listening properly.
        when(provider.getRestrictionChangeIntentAction())
                .thenReturn("org.chromium.test.policy.Hello");
        provider.startListeningForPolicyChanges();
        provider.stopListening();
        Assert.assertFalse(shadowApplication.hasReceiverForIntent(intent));
    }

}
