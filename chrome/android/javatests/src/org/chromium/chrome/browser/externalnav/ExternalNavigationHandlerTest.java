// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.SystemClock;
import android.provider.Browser;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.ui.base.PageTransition;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
public class ExternalNavigationHandlerTest extends InstrumentationTestCase {

    // Expectations
    private static final int IGNORE = 0x0;
    private static final int START_INCOGNITO = 0x1;
    private static final int START_ACTIVITY = 0x2;
    private static final int START_FILE = 0x4;
    private static final int INTENT_SANITIZATION_EXCEPTION = 0x8;

    private static final int NO_REDIRECT = 0x0;
    private static final int REDIRECT = 0x1;

    private static final String NO_REFERRER = null;

    private static final boolean NORMAL_PROFILE = false;
    private static final boolean INCOGNITO_PROFILE = true;

    private static final String SEARCH_RESULT_URL_FOR_TOM_HANKS =
            "https://www.google.com/search?q=tom+hanks";
    private static final String IMDB_WEBPAGE_FOR_TOM_HANKS = "http://m.imdb.com/name/nm0000158";
    private static final String INTENT_URL_WITH_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME =
            "intent:///name/nm0000158#Intent;scheme=imdb;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(IMDB_WEBPAGE_FOR_TOM_HANKS) + ";end";
    private static final String SOME_JAVASCRIPT_PAGE = "javascript:window.open(0);";
    private static final String INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL =
            "intent:///name/nm0000158#Intent;scheme=imdb;package=com.imdb.mobile;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode(SOME_JAVASCRIPT_PAGE) + ";end";
    private static final String IMDB_APP_INTENT_FOR_TOM_HANKS = "imdb:///name/nm0000158";
    private static final String INTENT_URL_WITH_CHAIN_FALLBACK_URL =
            "intent://scan/#Intent;scheme=zxing;"
            + "S." + ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL + "="
            + Uri.encode("http://url.myredirector.com/aaa") + ";end";

    private static final String PLUS_STREAM_URL = "https://plus.google.com/stream";
    private static final String CALENDAR_URL = "http://www.google.com/calendar";
    private static final String KEEP_URL = "http://www.google.com/keep";

    private static final String TEXT_APP_1_PACKAGE_NAME = "text_app_1";
    private static final String TEXT_APP_2_PACKAGE_NAME = "text_app_2";

    private final TestExternalNavigationDelegate mDelegate;
    private ExternalNavigationHandler mUrlHandler;

    public ExternalNavigationHandlerTest() {
        mDelegate = new TestExternalNavigationDelegate();
        mUrlHandler = new ExternalNavigationHandler(mDelegate);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate.setContext(getInstrumentation().getContext());
        CommandLine.init(new String[0]);
        RecordHistogram.disableForTests();
        mDelegate.mQueryIntentOverride = null;
    }

    @SmallTest
    public void testOrdinaryIncognitoUri() {
        check("http://youtube.com/",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                START_INCOGNITO);
    }

    @SmallTest
    public void testChromeReferrer() {
        // http://crbug.com/159153: Don't override http or https URLs from the NTP or bookmarks.
        check("http://youtube.com/",
                "chrome://about", /* referrer */
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check("tel:012345678",
                "chrome://about", /* referrer */
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testForwardBackNavigation() {
        // http://crbug.com/164194. We shouldn't show the intent picker on
        // forwards or backwards navigations.
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK
                | PageTransition.FORWARD_BACK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testRedirectFromFormSubmit() {
        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit. OAuth of native applications rely on this.
        check("market://1234",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        check("http://youtube.com://",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        // If the page matches the referrer, then continue loading in Chrome.
        check("http://youtube.com://",
                "http://youtube.com", /* referrer */
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        // If the page does not match the referrer, then prompt an intent.
        check("http://youtube.com://",
                "http://google.com", /* referrer */
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        // It doesn't make sense to allow intent picker without redirect, since form data
        // is not encoded in the intent (although, in theory, it could be passed in as
        // an extra data in the intent).
        check("http://youtube.com://",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testIgnore() {
        // Ensure about: URLs are not broadcast for external navigation.
        check("about:test",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check("about:test",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        // Ensure content: URLs are not broadcast for external navigation.
        check("content:test",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check("content:test",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        // Ensure chrome: URLs are not broadcast for external navigation.
        check("chrome://history",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check("chrome://history",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        // Ensure chrome-native: URLs are not broadcast for external navigation.
        check("chrome-native://newtab",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check("chrome-native://newtab",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testPageTransitionType() {
        // Non-link page transition type are ignored.
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        // http://crbug.com/143118 - Don't show the picker for directly typed URLs, unless
        // the URL results in a redirect.
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        // http://crbug.com/162106 - Don't show the picker on reload.
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.RELOAD,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testWtai() {
        // Start the telephone application with the given number.
        check("wtai://wp/mc;0123456789",
                NO_REFERRER,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY | INTENT_SANITIZATION_EXCEPTION);
        // These two cases are currently unimplemented.
        check("wtai://wp/sd;0123456789",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE | INTENT_SANITIZATION_EXCEPTION);
        check("wtai://wp/ap;0123456789",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE | INTENT_SANITIZATION_EXCEPTION);
        // Ignore other WTAI urls.
        check("wtai://wp/invalid",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE | INTENT_SANITIZATION_EXCEPTION);
    }

    @SmallTest
    public void testExternalUri() {
        check("tel:012345678",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testTypedRedirectToExternalProtocol() {
        // http://crbug.com/169549
        check("market://1234",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        // http://crbug.com/143118
        check("market://1234",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testIncomingIntentRedirect() {
        int transitionTypeIncomingIntent = PageTransition.LINK
                | PageTransition.FROM_API;
        // http://crbug.com/149218
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                transitionTypeIncomingIntent,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        // http://crbug.com/170925
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                transitionTypeIncomingIntent,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testIntentScheme() {
        String url = "intent:wtai://wp/#Intent;action=android.settings.SETTINGS;"
                + "component=package/class;end";
        String urlWithSel = "intent:wtai://wp/#Intent;SEL;action=android.settings.SETTINGS;"
                + "component=package/class;end";

        check(url,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);

        // http://crbug.com/370399
        check(urlWithSel,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testYouTubePairingCode() {
        int transitionTypeIncomingIntent = PageTransition.LINK
                | PageTransition.FROM_API;
        String url = "http://m.youtube.com/watch?v=1234&pairingCode=5678";

        // http://crbug/386600 - it makes no sense to switch activities for pairing code URLs.
        check(url,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        check(url,
                NO_REFERRER,
                NORMAL_PROFILE,
                transitionTypeIncomingIntent,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testInitialIntent() throws URISyntaxException {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(new TestContext());
        Intent ytIntent = Intent.parseUri("http://youtube.com/", Intent.URI_INTENT_SCHEME);
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        int transTypeLinkFromIntent = PageTransition.LINK
                | PageTransition.FROM_API;

        // Ignore if url is redirected, transition type is IncomingIntent and a new intent doesn't
        // have any new resolver.
        redirectHandler.updateIntent(ytIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                transTypeLinkFromIntent,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        // Do not ignore if a new intent has any new resolver.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                transTypeLinkFromIntent,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testInitialIntentHeadingToChrome() throws URISyntaxException {
        TestContext context = new TestContext();
        TabRedirectHandler redirectHandler = new TabRedirectHandler(context);
        Intent fooIntent = Intent.parseUri("http://foo.com/", Intent.URI_INTENT_SCHEME);
        fooIntent.setPackage(context.getPackageName());
        int transTypeLinkFromIntent = PageTransition.LINK
                | PageTransition.FROM_API;

        // Ignore if an initial Intent was heading to Chrome.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                transTypeLinkFromIntent,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        // Do not ignore if the URI has an external protocol.
        redirectHandler.updateIntent(fooIntent);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, false, false, 0, 0);
        redirectHandler.updateNewUrlLoading(transTypeLinkFromIntent, true, false, 0, 0);
        check("market://1234",
                NO_REFERRER,
                NORMAL_PROFILE,
                transTypeLinkFromIntent,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testFallbackUrl_IntentResolutionSucceeds() {
        // IMDB app is installed.
        mDelegate.setCanResolveActivity(true);

        check(INTENT_URL_WITH_FALLBACK_URL,
                SEARCH_RESULT_URL_FOR_TOM_HANKS, /* referrer */
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);

        Intent invokedIntent = mDelegate.startActivityIntent;
        assertEquals(IMDB_APP_INTENT_FOR_TOM_HANKS, invokedIntent.getData().toString());
        assertNull("The invoked intent should not have browser_fallback_url\n",
                invokedIntent.getStringExtra(ExternalNavigationHandler.EXTRA_BROWSER_FALLBACK_URL));
        assertNull(mDelegate.getNewUrlAfterClobbering());
        assertNull(mDelegate.getReferrerUrlForClobbering());
    }

    @SmallTest
    public void testFallbackUrl_IntentResolutionFails() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivity(false);

        // When intent resolution fails, we should not start an activity, but instead clobber
        // the current tab.
        check(INTENT_URL_WITH_FALLBACK_URL,
                SEARCH_RESULT_URL_FOR_TOM_HANKS, /* referrer */
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
                IGNORE);

        assertNull(mDelegate.startActivityIntent);
        assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @SmallTest
    public void testFallbackUrl_IntentResolutionFailsWithoutPackageName() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivity(false);

        // Fallback URL should work even when package name isn't given.
        check(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME,
                SEARCH_RESULT_URL_FOR_TOM_HANKS, /* referrer */
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
                IGNORE);

        assertNull(mDelegate.startActivityIntent);
        assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @SmallTest
    public void testFallbackUrl_FallbackShouldNotWarnOnIncognito() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivity(false);

        check(INTENT_URL_WITH_FALLBACK_URL,
                SEARCH_RESULT_URL_FOR_TOM_HANKS,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
                IGNORE);

        assertNull(mDelegate.startActivityIntent);
        assertEquals(IMDB_WEBPAGE_FOR_TOM_HANKS, mDelegate.getNewUrlAfterClobbering());
        assertEquals(SEARCH_RESULT_URL_FOR_TOM_HANKS, mDelegate.getReferrerUrlForClobbering());
    }

    @SmallTest
    public void testFallbackUrl_IgnoreJavascriptFallbackUrl() {
        // IMDB app isn't installed.
        mDelegate.setCanResolveActivity(false);

        // Will be redirected market since package is given.
        check(INTENT_URL_WITH_JAVASCRIPT_FALLBACK_URL,
                SEARCH_RESULT_URL_FOR_TOM_HANKS,
                INCOGNITO_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);

        Intent invokedIntent = mDelegate.startActivityIntent;
        assertTrue(invokedIntent.getData().toString().startsWith("market://"));
        assertEquals(null, mDelegate.getNewUrlAfterClobbering());
        assertEquals(null, mDelegate.getReferrerUrlForClobbering());
    }

    @SmallTest
    public void testFallback_UseFallbackUrlForRedirectionFromTypedInUrl() {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        check("http://goo.gl/abcdefg", NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED, NO_REDIRECT, true, false, redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        check(INTENT_URL_WITH_FALLBACK_URL_WITHOUT_PACKAGE_NAME, NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK, NO_REDIRECT, true, false, redirectHandler,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB, IGNORE);

        // Now the user opens a link.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 1);
        check("http://m.youtube.com/", NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK, NO_REDIRECT, true, false, redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE, IGNORE);
    }

    @SmallTest
    public void testIgnoreEffectiveRedirectFromIntentFallbackUrl() {
        // We cannot resolve any intent, so fall-back URL will be used.
        mDelegate.setCanResolveActivity(false);

        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, true, 0, 0);
        check(INTENT_URL_WITH_CHAIN_FALLBACK_URL,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
                IGNORE);

        // As a result of intent resolution fallback, we have clobberred the current tab.
        // The fall-back URL was HTTP-schemed, but it was effectively redirected to a new intent
        // URL using javascript. However, we do not allow chained fallback intent, so we do NOT
        // override URL loading here.
        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 0);
        check(INTENT_URL_WITH_FALLBACK_URL,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        // Now enough time (2 seconds) have passed.
        // New URL loading should not be affected.
        // (The URL happened to be the same as previous one.)
        // TODO(changwan): this is not likely cause flakiness, but it may be better to refactor
        // systemclock or pass the new time as parameter.
        long lastUserInteractionTimeInMillis = SystemClock.elapsedRealtime() + 2 * 1000L;
        redirectHandler.updateNewUrlLoading(
                PageTransition.LINK, false, true, lastUserInteractionTimeInMillis, 1);
        check(INTENT_URL_WITH_FALLBACK_URL,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB,
                IGNORE);
    }

    @SmallTest
    public void testIgnoreEffectiveRedirectFromUserTyping() {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, false, false, 0, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.TYPED, true, false, 0, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.TYPED,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 0, 1);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testNavigationFromLinkWithoutUserGesture() {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testChromeAppInBackground() {
        mDelegate.setIsChromeAppInForeground(false);
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testNotChromeAppInForegroundRequired() {
        mDelegate.setIsChromeAppInForeground(false);
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                false,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testCreatesIntentsToOpenInNewTab() {
        mUrlHandler = new ExternalNavigationHandler(mDelegate);
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(
                "http://m.youtube.com", false)
                .setOpenInNewTab(true)
                .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, result);
        assertTrue(mDelegate.startActivityIntent != null);
        assertTrue(mDelegate.startActivityIntent.getBooleanExtra(
                Browser.EXTRA_CREATE_NEW_TAB, false));
    }

    @SmallTest
    public void testCanExternalAppHandleUrl() {
        mDelegate.setCanResolveActivity(true);
        assertTrue(mUrlHandler.canExternalAppHandleUrl("some_app://some_app.com/"));

        mDelegate.setCanResolveActivity(false);
        assertTrue(mUrlHandler.canExternalAppHandleUrl("wtai://wp/mc;0123456789"));
        assertTrue(mUrlHandler.canExternalAppHandleUrl(
                "intent:/#Intent;scheme=no_app;package=com.no_app;end"));
        assertFalse(mUrlHandler.canExternalAppHandleUrl("no_app://no_app.com/"));
    }

    @SmallTest
    public void testPlusAppRefresh() {
        check(PLUS_STREAM_URL,
                PLUS_STREAM_URL,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testSameDomainDifferentApps() {
        check(CALENDAR_URL,
                KEEP_URL,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
    }

    @SmallTest
    public void testFormSubmitSameDomain() {
        check(CALENDAR_URL,
                KEEP_URL,
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testBackgroundTabNavigation() {
        check("http://youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                true,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testReferrerExtra() {
        String referrer = "http://www.google.com";
        check("http://youtube.com:90/foo/bar",
                referrer,
                NORMAL_PROFILE,
                PageTransition.FORM_SUBMIT,
                REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);
        assertEquals(Uri.parse(referrer),
                mDelegate.startActivityIntent.getParcelableExtra(Intent.EXTRA_REFERRER));
        assertEquals(1, mDelegate.startActivityIntent.getIntExtra(
                IntentHandler.EXTRA_REFERRER_ID, 0));
    }

    @SmallTest
    public void testNavigationFromReload() {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(PageTransition.RELOAD, false, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SmallTest
    public void testNavigationWithForwardBack() {
        TabRedirectHandler redirectHandler = new TabRedirectHandler(null);

        redirectHandler.updateNewUrlLoading(
                PageTransition.FORM_SUBMIT | PageTransition.FORWARD_BACK, false, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, true, false, 1, 0);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        redirectHandler.updateNewUrlLoading(PageTransition.LINK, false, false, 1, 1);
        check("http://m.youtube.com/",
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                redirectHandler,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
    }

    @SuppressLint("SdCardPath")
    @SmallTest
    public void testFileAccess() {
        String fileUrl = "file:///sdcard/Downloads/test.html";

        mDelegate.shouldRequestFileAccess = false;
        // Verify no overrides if file access is allowed (under different load conditions).
        check(fileUrl,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check(fileUrl,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.RELOAD,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);
        check(fileUrl,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.AUTO_TOPLEVEL,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.NO_OVERRIDE,
                IGNORE);

        mDelegate.shouldRequestFileAccess = true;
        // Verify that the file intent action is triggered if file access is not allowed.
        check(fileUrl,
                NO_REFERRER,
                NORMAL_PROFILE,
                PageTransition.AUTO_TOPLEVEL,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION,
                START_FILE);
    }

    @SmallTest
    public void testSms_DispatchIntentsToDefaultSmsApp() {
        final String referer = "https://www.google.com/";
        mDelegate.defaultSmsPackageName = TEXT_APP_2_PACKAGE_NAME;

        check("sms:+012345678?body=hello%20there",
                referer,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);

        assertNotNull(mDelegate.startActivityIntent);
        assertEquals(TEXT_APP_2_PACKAGE_NAME, mDelegate.startActivityIntent.getPackage());
    }

    @SmallTest
    public void testSms_DefaultSmsAppDoesNotHandleIntent() {
        final String referer = "https://www.google.com/";
        // Note that this package does not resolve the intent.
        mDelegate.defaultSmsPackageName = "text_app_3";

        check("sms:+012345678?body=hello%20there",
                referer,
                NORMAL_PROFILE,
                PageTransition.LINK,
                NO_REDIRECT,
                true,
                false,
                null,
                OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                START_ACTIVITY);

        assertNotNull(mDelegate.startActivityIntent);
        assertNull(mDelegate.startActivityIntent.getPackage());
    }

    private static ResolveInfo newResolveInfo(String packageName, String name) {
        ActivityInfo ai = new ActivityInfo();
        ai.packageName = packageName;
        ai.name = name;
        ResolveInfo ri = new ResolveInfo();
        ri.activityInfo = ai;
        return ri;
    }

    private static class TestExternalNavigationDelegate implements ExternalNavigationDelegate {
        private Context mContext;

        public void setContext(Context context) {
            mContext = context;
        }

        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent) {
            List<ResolveInfo> list = new ArrayList<ResolveInfo>();
            // TODO(yfriedman): We shouldn't have a separate global override just for tests - we
            // should mimic the appropriate intent resolution intead.
            if (mQueryIntentOverride != null) {
                if (mQueryIntentOverride.booleanValue()) {
                    list.add(newResolveInfo("foo", "foo"));
                } else {
                    return list;
                }
            }
            if (intent.getDataString().startsWith("http://m.youtube.com")
                    || intent.getDataString().startsWith("http://youtube.com")) {
                list.add(newResolveInfo("youtube", "youtube"));
            } else if (intent.getDataString().startsWith(PLUS_STREAM_URL)) {
                list.add(newResolveInfo("plus", "plus"));
            } else if (intent.getDataString().startsWith(CALENDAR_URL)) {
                list.add(newResolveInfo("calendar", "calendar"));
            } else if (intent.getDataString().startsWith("sms")) {
                list.add(newResolveInfo(
                        TEXT_APP_1_PACKAGE_NAME, TEXT_APP_1_PACKAGE_NAME + ".cls"));
                list.add(newResolveInfo(
                        TEXT_APP_2_PACKAGE_NAME, TEXT_APP_2_PACKAGE_NAME + ".cls"));
            } else {
                list.add(newResolveInfo("foo", "foo"));
            }
            return list;
        }

        @Override
        public boolean willChromeHandleIntent(Intent intent) {
            return !isSpecializedHandlerAvailable(queryIntentActivities(intent));
        }

        @Override
        public boolean isSpecializedHandlerAvailable(List<ResolveInfo> resolveInfos) {
            for (ResolveInfo resolveInfo : resolveInfos) {
                String packageName = resolveInfo.activityInfo.packageName;
                if (packageName.equals("youtube") || packageName.equals("calendar")) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public String getPackageName() {
            return "test";
        }

        @Override
        public void startActivity(Intent intent) {
            startActivityIntent = intent;
        }

        @Override
        public boolean startActivityIfNeeded(Intent intent) {
            // For simplicity, don't distinguish between startActivityIfNeeded and startActivity
            // until a test requires this distinction.
            startActivityIntent = intent;
            return true;
        }

        @Override
        public void startIncognitoIntent(Intent intent, String referrerUrl, String fallbackUrl,
                Tab tab, boolean needsToCloseTab) {
            startIncognitoIntentCalled = true;
        }

        @Override
        public boolean shouldRequestFileAccess(String url, Tab tab) {
            return shouldRequestFileAccess;
        }

        @Override
        public void startFileIntent(Intent intent, String referrerUrl, Tab tab,
                boolean needsToCloseTab) {
            startFileIntentCalled = true;
        }

        @Override
        public OverrideUrlLoadingResult clobberCurrentTab(
                String url, String referrerUrl, Tab tab) {
            mNewUrlAfterClobbering = url;
            mReferrerUrlForClobbering = referrerUrl;
            return OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB;
        }

        @Override
        public boolean isChromeAppInForeground() {
            return mIsChromeAppInForeground;
        }

        @Override
        public boolean isDocumentMode() {
            return FeatureUtilities.isDocumentMode(mContext);
        }

        @Override
        public String getDefaultSmsPackageName() {
            return defaultSmsPackageName;
        }

        public void reset() {
            startActivityIntent = null;
            startIncognitoIntentCalled = false;
            startFileIntentCalled = false;
        }

        public void setCanResolveActivity(boolean value) {
            mQueryIntentOverride = value;
        }

        public String getNewUrlAfterClobbering() {
            return mNewUrlAfterClobbering;
        }

        public String getReferrerUrlForClobbering() {
            return mReferrerUrlForClobbering;
        }

        public void setIsChromeAppInForeground(boolean value) {
            mIsChromeAppInForeground = value;
        }

        public Intent startActivityIntent = null;
        public boolean startIncognitoIntentCalled = false;

        // This should not be reset for every run of check().
        private Boolean mQueryIntentOverride;

        private String mNewUrlAfterClobbering;
        private String mReferrerUrlForClobbering;
        public boolean mIsChromeAppInForeground = true;

        public boolean shouldRequestFileAccess;
        public boolean startFileIntentCalled;
        public String defaultSmsPackageName;
    }

    private void checkIntentSanity(Intent intent, String name) {
        assertTrue("The invoked " + name + " doesn't have the BROWSABLE category set\n",
                intent.hasCategory(Intent.CATEGORY_BROWSABLE));
        assertNull("The invoked " + name + " should not have a Component set\n",
                intent.getComponent());
    }

    public void check(String url,
                      String referrerUrl,
                      boolean isIncognito,
                      int pageTransition,
                      int isRedirect,
                      boolean chromeAppInForegroundRequired,
                      boolean isBackgroundTabNavigation,
                      TabRedirectHandler redirectHandler,
                      OverrideUrlLoadingResult expectedOverrideResult,
                      int otherExpectation) {
        boolean expectStartIncognito = (otherExpectation & START_INCOGNITO) != 0;
        boolean expectStartActivity = (otherExpectation & START_ACTIVITY) != 0;
        boolean expectStartFile = (otherExpectation & START_FILE) != 0;
        boolean expectSaneIntent = (otherExpectation & INTENT_SANITIZATION_EXCEPTION) == 0;

        mDelegate.reset();

        ExternalNavigationParams params = new ExternalNavigationParams.Builder(url, isIncognito,
                referrerUrl, pageTransition, isRedirect == REDIRECT)
                .setApplicationMustBeInForeground(chromeAppInForegroundRequired)
                .setRedirectHandler(redirectHandler)
                .setIsBackgroundTabNavigation(isBackgroundTabNavigation)
                .setIsMainFrame(true)
                .build();
        OverrideUrlLoadingResult result = mUrlHandler.shouldOverrideUrlLoading(params);
        boolean startActivityCalled = mDelegate.startActivityIntent != null;

        assertEquals(expectedOverrideResult, result);
        assertEquals(expectStartIncognito, mDelegate.startIncognitoIntentCalled);
        assertEquals(expectStartActivity, startActivityCalled);
        assertEquals(expectStartFile, mDelegate.startFileIntentCalled);

        if (startActivityCalled && expectSaneIntent) {
            checkIntentSanity(mDelegate.startActivityIntent, "Intent");
            if (mDelegate.startActivityIntent.getSelector() != null) {
                checkIntentSanity(mDelegate.startActivityIntent.getSelector(),
                        "Intent's selector");
            }
        }
    }

    private static class TestPackageManager extends MockPackageManager {
        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo> resolves = new ArrayList<ResolveInfo>();
            if (intent.getDataString().startsWith("market:")) {
                resolves.add(newResolveInfo("market", "market"));
            } else if (intent.getDataString().startsWith("http://m.youtube.com")
                    ||  intent.getDataString().startsWith("http://youtube.com")) {
                resolves.add(newResolveInfo("youtube", "youtube"));
            } else {
                resolves.add(newResolveInfo("foo", "foo"));
            }
            return resolves;
        }
    }

    private static class TestContext extends MockContext {
        @Override
        public PackageManager getPackageManager() {
            return new TestPackageManager();
        }

        @Override
        public String getPackageName() {
            return "test.app.name";
        }

    }
}
