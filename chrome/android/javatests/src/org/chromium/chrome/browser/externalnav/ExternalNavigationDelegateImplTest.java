// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.support.test.filters.SmallTest;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
public class ExternalNavigationDelegateImplTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public ExternalNavigationDelegateImplTest() {
        super(ChromeActivity.class);
    }

    private static List<ResolveInfo> makeResolveInfos(ResolveInfo... infos) {
        return Arrays.asList(infos);
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_NoResolveInfo() {
        String packageName = "";
        List<ResolveInfo> resolveInfos = new ArrayList<ResolveInfo>();
        assertEquals(0, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_NoPathOrAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        assertEquals(0, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_WithPath() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        assertEquals(1, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        assertEquals(1, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_Matching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = packageName;
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        assertEquals(1, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_NotMatching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = "com.foo.bar";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        assertEquals(0, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsPackageSpecializeHandler_withEphemeralResolver() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        info.activityInfo = new ActivityInfo();
        info.activityInfo.name = InstantAppsHandler.EPHEMERAL_INSTALLER_CLASS;
        info.activityInfo.packageName = "com.google.android.gms";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        // Ephemeral resolver is not counted as a specialized handler.
        assertEquals(0, ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(
                resolveInfos, packageName).size());
    }

    @SmallTest
    public void testIsDownload_noSystemDownloadManager() throws Exception {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                getActivity().getActivityTab());
        assertTrue("pdf should be a download, no viewer in Android Chrome",
                delegate.isPdfDownload("http://somesampeleurldne.com/file.pdf"));
        assertFalse("URL is not a file, but web page",
                delegate.isPdfDownload("http://somesampleurldne.com/index.html"));
        assertFalse("URL is not a file url",
                delegate.isPdfDownload("http://somesampeleurldne.com/not.a.real.extension"));
        assertFalse("URL is an image, can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/image.jpg"));
        assertFalse("URL is a text file can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/copy.txt"));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
