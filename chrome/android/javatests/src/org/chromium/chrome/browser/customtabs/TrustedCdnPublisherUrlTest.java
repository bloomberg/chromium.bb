// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.init.ChromeBrowserInitializer.PRIVATE_DATA_DIRECTORY_SUFFIX;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.view.View;
import android.widget.ImageView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.AnnotationRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for showing the publisher URL for a trusted CDN.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedCdnPublisherUrlTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String PAGE_WITH_TITLE =
            "<!DOCTYPE html><html><head><title>Example title</title></head></html>";

    private TestWebServer mWebServer;

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    /**
     * Annotation to override the trusted CDN.
     */
    @Retention(RetentionPolicy.RUNTIME)
    private @interface OverrideTrustedCdn {}

    private static class OverrideTrustedCdnRule extends AnnotationRule {
        public OverrideTrustedCdnRule() {
            super(OverrideTrustedCdn.class);
        }

        /**
         * @return Whether the trusted CDN should be overridden.
         */
        public boolean isEnabled() {
            return !getAnnotations().isEmpty();
        }
    }

    @Rule
    public OverrideTrustedCdnRule mOverrideTrustedCdn = new OverrideTrustedCdnRule();

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        mWebServer = TestWebServer.start();
        if (mOverrideTrustedCdn.isEnabled()) {
            CommandLine.getInstance().appendSwitchWithValue(
                    "trusted-cdn-base-url-for-tests", mWebServer.getBaseUrl());
        }
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testHttps() throws Exception {
        runTrustedCdnPublisherUrlTest("https://example.com/test", "com.example.test", "example.com",
                org.chromium.chrome.R.drawable.omnibox_https_valid);
        mScreenShooter.shoot("trustedPublisherUrlHttps");
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testHttp() throws Exception {
        runTrustedCdnPublisherUrlTest("http://example.com/test", "com.example.test", "example.com",
                org.chromium.chrome.R.drawable.omnibox_info);
        mScreenShooter.shoot("trustedPublisherUrlHttp");
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testRtl() throws Exception {
        String publisher = "\u200e\u202b\u0645\u0648\u0642\u0639\u002e\u0648\u0632\u0627\u0631"
                + "\u0629\u002d\u0627\u0644\u0623\u062a\u0635\u0627\u0644\u0627\u062a\u002e\u0645"
                + "\u0635\u0631\u202c\u200e";
        runTrustedCdnPublisherUrlTest("http://xn--4gbrim.xn----rmckbbajlc6dj7bxne2c.xn--wgbh1c/",
                "com.example.test", publisher, org.chromium.chrome.R.drawable.omnibox_info);
        mScreenShooter.shoot("trustedPublisherUrlRtl");
    }

    private int getDefaultSecurityIcon() {
        // On tablets an info icon is shown for ConnectionSecurityLevel.NONE pages,
        // on smaller form factors nothing.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    InstrumentationRegistry.getTargetContext())) {
            return org.chromium.chrome.R.drawable.omnibox_info;
        }

        return 0;
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testUntrustedClient() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.someoneelse.bla", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testNoHeader() throws Exception {
        runTrustedCdnPublisherUrlTest(null, "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testMalformedHeader() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "garbage", "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.DisableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testDisabled() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    // No @OverrideTrustedCdn
    public void testUntrustedCdn() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.example.test", null, getDefaultSecurityIcon());
    }
    // TODO(bauerb): Test an insecure HTTPS connection.

    private void runTrustedCdnPublisherUrlTest(@Nullable String publisherUrl, String clientPackage,
            @Nullable String expectedPublisher, int expectedSecurityIcon)
            throws InterruptedException, TimeoutException {
        final List<Pair<String, String>> headers;
        if (publisherUrl != null) {
            headers = Collections.singletonList(Pair.create("X-AMP-Cache", publisherUrl));
        } else {
            headers = null;
        }
        String testUrl = mWebServer.setResponse("/test.html", PAGE_WITH_TITLE, headers);
        Context targetContext = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(targetContext, testUrl);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, clientPackage);
        connection.setTrustedPublisherUrlPackageForTest("com.example.test");

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        UrlBar urlBar = mCustomTabActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.R.id.url_bar);
        final String expectedUrl;
        if (expectedPublisher == null) {
            expectedUrl = UrlFormatter.formatUrlForSecurityDisplay(testUrl, false);
        } else {
            expectedUrl =
                    String.format(Locale.US, "From %s – delivered by Google", expectedPublisher);
        }
        Assert.assertEquals(expectedUrl, urlBar.getText().toString());

        ImageView securityButton = mCustomTabActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.R.id.security_button);
        if (expectedSecurityIcon == 0) {
            Assert.assertEquals(View.INVISIBLE, securityButton.getVisibility());
        } else {
            Assert.assertEquals(View.VISIBLE, securityButton.getVisibility());
            Bitmap expectedIcon = BitmapFactory.decodeResource(
                    targetContext.getResources(), expectedSecurityIcon);
            Assert.assertTrue(expectedIcon.sameAs(
                    ((BitmapDrawable) securityButton.getDrawable()).getBitmap()));
        }
    }
}
