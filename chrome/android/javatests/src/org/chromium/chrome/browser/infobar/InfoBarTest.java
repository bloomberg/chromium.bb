// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarListeners;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;


import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

public class InfoBarTest extends ChromiumTestShellTestBase {
    private final static int MAX_TIMEOUT = 2000;
    private final static int CHECK_INTERVAL = 500;
    private final static String GEOLOCAION_PAGE =
            "chrome/test/data/geolocation/geolocation_on_load.html";
    public final static String HELLO_WORLD_URL = UrlUtils.encodeHtmlDataUri(
            "<html>" +
            "<head><title>Hello, World!</title></head>" +
            "<body>Hello, World!</body>" +
            "</html>");

    private InfoBarTestAnimationListener mListener;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Register for animation notifications
        InfoBarContainer container =
                getActivity().getActiveTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
    }

    /**
     * Verify Geolocation creates an InfoBar.
     */
    @Smoke
    @MediumTest
    @Feature({"Browser", "Main"})
    public void testInfoBarForGeolocation() throws InterruptedException {
        loadUrlWithSanitization(TestHttpServerClient.getUrl(GEOLOCAION_PAGE));
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Make sure it has OK/Cancel buttons.
        List<InfoBar> infoBars = getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());
        InfoBarUtil.hasPrimaryButton(this, infoBars.get(0));
        InfoBarUtil.hasSecondaryButton(this, infoBars.get(0));

        loadUrlWithSanitization(HELLO_WORLD_URL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        infoBars = getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        assertTrue("Wrong infobar count", infoBars.isEmpty());
    }


    /**
     * Verify Geolocation creates an InfoBar and that it's destroyed when navigating back.
     *
     */
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarForGeolocationDisappearsOnBack() throws InterruptedException {
        loadUrlWithSanitization(HELLO_WORLD_URL);
        loadUrlWithSanitization(TestHttpServerClient.getUrl(GEOLOCAION_PAGE));
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        List<InfoBar> infoBars = getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
        assertEquals("Wrong infobar count", 1, infoBars.size());

        // Navigate back and ensure the InfoBar has been removed.
        getInstrumentation().runOnMainSync(
            new Runnable() {
                @Override
                public void run() {
                    getActivity().getActiveTab().goBack();
                }
            });
        CriteriaHelper.pollForCriteria(
            new Criteria() {
                @Override
                public boolean isSatisfied() {
                    List<InfoBar> infoBars =
                            getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
                    return infoBars.isEmpty();
                }
            },
            MAX_TIMEOUT, CHECK_INTERVAL);
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
    }
}
