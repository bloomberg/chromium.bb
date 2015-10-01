// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * WebContentsObserver used by Tab.
 */
public class TabWebContentsObserver extends WebContentsObserver {
    // URL didFailLoad error code. Should match the value in net_error_list.h.
    public static final int BLOCKED_BY_ADMINISTRATOR = -22;

    private final ChromeTab mTab;

    public TabWebContentsObserver(WebContents webContents, ChromeTab tab) {
        super(webContents);
        mTab = tab;
    }

    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        PolicyAuditor auditor =
                ((ChromeApplication) mTab.getApplicationContext()).getPolicyAuditor();
        auditor.notifyAuditEvent(
                mTab.getApplicationContext(), AuditEvent.OPEN_URL_SUCCESS, validatedUrl, "");
    }

    @Override
    public void didFailLoad(boolean isProvisionalLoad, boolean isMainFrame, int errorCode,
            String description, String failingUrl, boolean wasIgnoredByHandler) {
        PolicyAuditor auditor =
                ((ChromeApplication) mTab.getApplicationContext()).getPolicyAuditor();
        auditor.notifyAuditEvent(mTab.getApplicationContext(), AuditEvent.OPEN_URL_FAILURE,
                failingUrl, description);
        if (errorCode == BLOCKED_BY_ADMINISTRATOR) {
            auditor.notifyAuditEvent(
                    mTab.getApplicationContext(), AuditEvent.OPEN_URL_BLOCKED, failingUrl, "");
        }
    }

    @Override
    public void didCommitProvisionalLoadForFrame(
            long frameId, boolean isMainFrame, String url, int transitionType) {
        if (!isMainFrame) return;
        mTab.handleDidCommitProvisonalLoadForFrame(url, transitionType);

        if (mTab.getInterceptNavigationDelegate() != null) {
            mTab.getInterceptNavigationDelegate().maybeUpdateNavigationHistory();
        }
    }

    @Override
    public void didAttachInterstitialPage() {
        PolicyAuditor auditor =
                ((ChromeApplication) mTab.getApplicationContext()).getPolicyAuditor();
        auditor.notifyCertificateFailure(mTab.getWebContents(), mTab.getApplicationContext());
    }

    @Override
    public void didDetachInterstitialPage() {
        if (!mTab.maybeShowNativePage(mTab.getUrl(), false)) {
            mTab.showRenderedPage();
        }
    }

    @Override
    public void destroy() {
        MediaCaptureNotificationService.updateMediaNotificationForTab(
                mTab.getApplicationContext(), mTab.getId(), false, false, mTab.getUrl());
        super.destroy();
    }
}