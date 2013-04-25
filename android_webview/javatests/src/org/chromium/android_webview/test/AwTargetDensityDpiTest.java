// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.ui.gfx.DeviceDisplayInfo;

public class AwTargetDensityDpiTest extends AwTestBase {

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTargetDensityDpi() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String pageTemplate = "<html><head>" +
                "<meta name='viewport' content='width=device-width, target-densityDpi=%s' />" +
                "</head><body onload='document.title=document.body.clientWidth'></body></html>";
        final String pageDeviceDpi = String.format(pageTemplate, "device-dpi");
        final String pageHighDpi = String.format(pageTemplate, "high-dpi");
        final String pageDpi100 = String.format(pageTemplate, "100");

        settings.setJavaScriptEnabled(true);

        DeviceDisplayInfo deviceInfo =
                DeviceDisplayInfo.create(getInstrumentation().getTargetContext());
        loadDataSync(awContents, onPageFinishedHelper, pageDeviceDpi, "text/html", false);
        int actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertEquals((float)deviceInfo.getDisplayWidth(), (float)actualWidth, 10f);

        float displayWidth = (float)(deviceInfo.getDisplayWidth() / deviceInfo.getDIPScale());
        float deviceDpi = (float)(120f * deviceInfo.getDIPScale());
        loadDataSync(awContents, onPageFinishedHelper, pageHighDpi, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertEquals(displayWidth * (240f / deviceDpi), (float)actualWidth, 10f);

        loadDataSync(awContents, onPageFinishedHelper, pageDpi100, "text/html", false);
        actualWidth = Integer.parseInt(getTitleOnUiThread(awContents));
        assertEquals(displayWidth * (100f / deviceDpi), (float)actualWidth, 10f);
    }
}
