// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * PaymentHandler coordinator, which owns the component overall, i.e., creates other objects in the
 * component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class PaymentHandlerCoordinator {
    private Runnable mHider;

    /** Observer for the dismissal of the payment-handler UI. */
    public interface DismissObserver {
        /**
         * Called after the user has dismissed the payment-handler UI by swiping it down or tapping
         * on the scrim behind the UI.
         */
        void onDismissed();
    }

    /** Constructs the payment-handler component coordinator. */
    public PaymentHandlerCoordinator() {
        assert isEnabled();
    }

    /**
     * Shows the payment-handler UI.
     *
     * @param chromeActivity  The activity where the UI should be shown.
     * @param dismissObserver The observer to be notified when the user has dismissed the UI.
     * @return Whether the payment-handler UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(ChromeActivity activity, DismissObserver dismissObserver) {
        assert mHider == null : "Already showing payment-handler UI";
        PaymentHandlerMediator mediator = new PaymentHandlerMediator(this::hide, dismissObserver);
        BottomSheetController bottomSheetController = activity.getBottomSheetController();
        bottomSheetController.getBottomSheet().addObserver(mediator);
        PropertyModel model = new PropertyModel.Builder(PaymentHandlerProperties.ALL_KEYS).build();
        PaymentHandlerView view = new PaymentHandlerView(activity);
        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, PaymentHandlerViewBinder::bind);
        mHider = () -> {
            changeProcessor.destroy();
            bottomSheetController.getBottomSheet().removeObserver(mediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
        };
        boolean result = bottomSheetController.requestShowContent(view, /*animate=*/true);
        if (result) bottomSheetController.expandSheet();
        return result;
    }

    /** Hides the payment-handler UI. */
    public void hide() {
        if (mHider == null) return;
        mHider.run();
    }

    /**
     * @return Whether this solution (as opposed to the Chrome-custom-tab based solution) of
     *     PaymentHandler is enabled. This solution is intended to replace the other
     *     solution.
     */
    public static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SCROLL_TO_EXPAND_PAYMENT_HANDLER);
    }
}
