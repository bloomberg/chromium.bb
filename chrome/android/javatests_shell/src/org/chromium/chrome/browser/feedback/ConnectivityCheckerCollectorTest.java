// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for {@link ConnectivityCheckerCollector}.
 */
public class ConnectivityCheckerCollectorTest extends ConnectivityCheckerTestBase {
    private static final int RESULT_CHECK_INTERVAL_MS = 10;
    public static final String TAG = "Feedback";

    @MediumTest
    public void testNormalCaseShouldWork() throws Exception {
        final AtomicReference<Future<Map<ConnectivityCheckerCollector.Type,
                ConnectivityCheckerCollector.Result>>> resultFuture = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Intentionally make HTTPS-connection fail which should result in NOT_CONNECTED.
                ConnectivityChecker.overrideUrlsForTest(GENERATE_204_URL, GENERATE_404_URL);

                resultFuture.set(ConnectivityCheckerCollector.startChecks(
                        Profile.getLastUsedProfile(), TIMEOUT_MS));
            }
        });

        boolean gotResult = CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return resultFuture.get().isDone();
            }
        }, TIMEOUT_MS, RESULT_CHECK_INTERVAL_MS);
        assertTrue("Should be finished by now.", gotResult);
        Map<ConnectivityCheckerCollector.Type, ConnectivityCheckerCollector.Result> result =
                getResult(resultFuture);
        assertEquals("Should have 4 results.", 4, result.size());
        for (Map.Entry<ConnectivityCheckerCollector.Type, ConnectivityCheckerCollector.Result> re :
                result.entrySet()) {
            switch (re.getKey()) {
                case CHROME_HTTP:
                case SYSTEM_HTTP:
                    assertEquals("Wrong result for " + re.getKey(),
                            ConnectivityCheckerCollector.Result.CONNECTED, re.getValue());
                    break;
                case CHROME_HTTPS:
                case SYSTEM_HTTPS:
                    assertEquals("Wrong result for " + re.getKey(),
                            ConnectivityCheckerCollector.Result.NOT_CONNECTED, re.getValue());
                    break;
                default:
                    fail("Failed to recognize type " + re.getKey());
            }
        }
    }

    @MediumTest
    public void testTwoTimeoutsShouldFillInTheRest() throws Exception {
        final AtomicReference<Future<Map<ConnectivityCheckerCollector.Type,
                ConnectivityCheckerCollector.Result>>> resultFuture = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Intentionally make HTTPS connections slow which should result in UNKNOWN.
                ConnectivityChecker.overrideUrlsForTest(GENERATE_204_URL, GENERATE_204_SLOW_URL);

                resultFuture.set(ConnectivityCheckerCollector.startChecks(
                        Profile.getLastUsedProfile(), TIMEOUT_MS));
            }
        });

        boolean gotResult = CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return resultFuture.get().isDone();
            }
        }, TIMEOUT_MS / 5, RESULT_CHECK_INTERVAL_MS);
        assertFalse("Should not be finished by now.", gotResult);
        Map<ConnectivityCheckerCollector.Type, ConnectivityCheckerCollector.Result> result =
                getResult(resultFuture);

        assertEquals("Should have 4 results.", 4, result.size());
        for (Map.Entry<ConnectivityCheckerCollector.Type, ConnectivityCheckerCollector.Result> re :
                result.entrySet()) {
            switch (re.getKey()) {
                case CHROME_HTTP:
                case SYSTEM_HTTP:
                    assertEquals("Wrong result for " + re.getKey(),
                            ConnectivityCheckerCollector.Result.CONNECTED, re.getValue());
                    break;
                case CHROME_HTTPS:
                case SYSTEM_HTTPS:
                    assertEquals("Wrong result for " + re.getKey(),
                            ConnectivityCheckerCollector.Result.UNKNOWN, re.getValue());
                    break;
                default:
                    fail("Failed to recognize type " + re.getKey());
            }
        }
    }

    private static Map<ConnectivityCheckerCollector.Type, ConnectivityCheckerCollector.Result>
            getResult(final AtomicReference<Future<Map<ConnectivityCheckerCollector.Type,
            ConnectivityCheckerCollector.Result>>> resultFuture) {
        final AtomicReference<Map<ConnectivityCheckerCollector.Type,
                ConnectivityCheckerCollector.Result>> result = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    result.set(resultFuture.get().get());
                } catch (ExecutionException | InterruptedException e) {
                    // Intentionally ignored.
                }
            }
        });
        return result.get();
    }
}
