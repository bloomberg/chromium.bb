// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.os.Handler;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.SslValidityChecker;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator
        extends WebContentsObserver implements BottomSheetObserver, PaymentHandlerToolbarObserver {
    private final PropertyModel mModel;
    private final Runnable mHider;
    // Postfixed with "Ref" to distinguish from mWebContent in WebContentsObserver. Although
    // referencing the same object, mWebContentsRef is preferable to WebContents here because
    // mWebContents (a weak ref) requires null checks, while mWebContentsRef is guaranteed to be not
    // null.
    private final WebContents mWebContentsRef;
    private final PaymentHandlerUiObserver mPaymentHandlerUiObserver;
    private final Handler mHandler = new Handler();

    /**
     * Build a new mediator that handle events from outside the payment handler component.
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *         payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *         hidden.
     * @param webContents The web-contents that loads the payment app.
     * @param observer The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     */
    /* package */ PaymentHandlerMediator(PropertyModel model, Runnable hider,
            WebContents webContents, PaymentHandlerUiObserver observer) {
        super(webContents);
        assert webContents != null;
        mWebContentsRef = webContents;
        mModel = model;
        mHider = hider;
        mPaymentHandlerUiObserver = observer;
    }

    // BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
                mHandler.post(mHider);
                break;
        }
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        mModel.set(PaymentHandlerProperties.BOTTOM_SHEET_HEIGHT_FRACTION, heightFraction);
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mPaymentHandlerUiObserver.onPaymentHandlerUiShown();
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    @Override
    public void onSheetFullyPeeked() {}

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // WebContentsObserver:
    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mWebContentsRef)) {
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                    mWebContentsRef);
            mHandler.post(mHider);
        }
    }

    @Override
    public void didAttachInterstitialPage() {
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                mWebContentsRef);
        mHandler.post(mHider);
    }

    @Override
    public void didFailLoad(
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
        // TODO(crbug.com/1017926): Respond to service worker with the net error.
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
        mHandler.post(mHider);
    }

    // PaymentHandlerToolbarObserver:
    @Override
    public void onToolbarCloseButtonClicked() {
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
        mHandler.post(mHider);
    }

    @Override
    public void onToolbarError() {
        // TODO(maxlg): send an error message to users.
        ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
        mHandler.post(mHider);
    }
}
