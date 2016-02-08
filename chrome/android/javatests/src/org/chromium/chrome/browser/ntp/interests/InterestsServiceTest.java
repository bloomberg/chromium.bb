// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.interests;

import android.accounts.Account;
import android.content.Context;
import android.test.FlakyTest;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ntp.interests.InterestsService.Interest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.AccountTrackerService;
import org.chromium.chrome.browser.signin.OAuth2TokenService;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.sync.signin.ChromeSigninController;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.SynchronousQueue;

/**
 * Tests for InterestsService.
 */
public class InterestsServiceTest extends NativeLibraryTestBase {
    // Special Interest array to signal a null response since LinkedBlockingQueue does not
    // accept null.
    private static final Interest[] NULL_RESPONSE = new Interest[0];

    private Context mContext;
    private Account mAccount;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();

        mContext = getInstrumentation().getTargetContext();

        // Create the test account and wait for it to be seeded fully.
        final CountDownLatch latch = new CountDownLatch(1);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                AccountTrackerService.get(mContext).addSystemAccountsSeededListener(
                        new AccountTrackerService.OnSystemAccountsSeededListener() {
                            @Override
                            public void onSystemAccountsSeedingComplete() {
                                latch.countDown();
                            }

                            @Override
                            public void onSystemAccountsChanged() {}
                        });
                SigninTestUtil.setUpAuthForTest(getInstrumentation());
                mAccount = SigninTestUtil.get().addAndSignInTestAccount();
            }
        });

        latch.await();

        // Sign in with the test account.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(mContext).onFirstRunCheckDone();
                SigninManager.get(mContext).signIn(mAccount, null, null);
            }
        });
    }

    @Override
    public void tearDown() throws Exception {
        // Sign out of the test account.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeSigninController.get(mContext).setSignedInAccountName(null);
                OAuth2TokenService.getForProfile(Profile.getLastUsedProfile())
                        .validateAccounts(mContext, false);
            }
        });
        super.tearDown();
    }

    /**
     * http://crbug.com/585173
     * @SmallTest
     * @Feature({"NewTabPage"})
     */
    @FlakyTest
    public void testEmptyInterests() throws Exception {
        String response = "{\n"
                + "  \"interests\": []\n"
                + "}\n";
        Interest[] result = serveResponseAndRequestInterests(response);

        assertTrue(NULL_RESPONSE != result);
        assertEquals(0, result.length);
    }

    /**
     * http://crbug.com/585173
     * @SmallTest
     * @Feature({"NewTabPage"})
     */
    @FlakyTest
    public void testInterests() throws Exception {
        String response = "{\n"
                + "  \"interests\": [\n"
                + "    {\n"
                + "      \"name\": \"Google\",\n"
                + "      \"imageUrl\": \"https://fake.com/fake.png\",\n"
                + "      \"relevance\": 0.9\n"
                + "    },\n"
                + "    {\n"
                + "      \"name\": \"Google Chrome\",\n"
                + "      \"imageUrl\": \"https://fake.com/fake.png\",\n"
                + "      \"relevance\": 0.98\n"
                + "    }\n"
                + "  ]\n"
                + "}\n";
        Interest[] result = serveResponseAndRequestInterests(response);

        assertEquals(2, result.length);
        assertEquals(new Interest("Google", "https://fake.com/fake.png", 0.9), result[0]);
        assertEquals(new Interest("Google Chrome", "https://fake.com/fake.png", 0.98), result[1]);
    }

    /**
     * http://crbug.com/585173
     * @SmallTest
     * @Feature({"NewTabPage"})
     */
    @FlakyTest
    public void testBadlyFormedInterests() throws Exception {
        String response = "{\n"
                + "  \"interests\": [";
        Interest[] result = serveResponseAndRequestInterests(response);

        assertTrue(NULL_RESPONSE == result);
    }

    /**
     * http://crbug.com/585173
     * @SmallTest
     * @Feature({"NewTabPage"})
     */
    @FlakyTest
    public void testEmptyResponse() throws Exception {
        Interest[] result = serveResponseAndRequestInterests("");

        assertTrue(NULL_RESPONSE == result);
    }

    private Interest[] serveResponseAndRequestInterests(String response) throws Exception {
        TestWebServer server = TestWebServer.start();
        String url = server.setResponse("/", response, null);
        CommandLine.getInstance().appendSwitchWithValue("interests-url", url);

        final BlockingQueue<Interest[]> queue = new SynchronousQueue<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                InterestsService is = new InterestsService(Profile.getLastUsedProfile());
                is.getInterests(new InterestsService.GetInterestsCallback() {
                    @Override
                    public void onInterestsAvailable(Interest[] interests) {
                        // We can't put a null on the queue, so use we'll use a global.
                        try {
                            if (interests == null) {
                                queue.put(NULL_RESPONSE);
                            } else {
                                queue.put(interests);
                            }
                        } catch (InterruptedException e) {
                            fail("Could not pass interests back to test thread");
                        }
                    }
                });
            }
        });

        // Wait for the interests to be fetched before shutting the server down.
        Interest[] result = queue.take();
        server.shutdown();

        return result;
    }
}
