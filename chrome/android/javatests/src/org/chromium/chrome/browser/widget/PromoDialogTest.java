// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.content.DialogInterface;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.PromoDialog.DialogParams;
import org.chromium.chrome.test.util.ApplicationTestUtils;

import java.util.concurrent.Callable;

/**
 * Tests for the PromoDialog and PromoDialogLayout.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PromoDialogTest {
    /**
     * Creates a PromoDialog.  Doesn't call {@link PromoDialog#show} because there is no Window to
     * attach them to, but it does create them and inflate the layouts.
     */
    private static class PromoDialogWrapper implements Callable<PromoDialog> {
        public final CallbackHelper primaryCallback = new CallbackHelper();
        public final CallbackHelper secondaryCallback = new CallbackHelper();
        public final PromoDialog dialog;

        private final Context mContext;
        private final DialogParams mDialogParams;

        PromoDialogWrapper(final DialogParams dialogParams) throws Exception {
            mContext = InstrumentationRegistry.getTargetContext();
            mDialogParams = dialogParams;
            dialog = ThreadUtils.runOnUiThreadBlocking(this);
        }

        @Override
        public PromoDialog call() {
            PromoDialog dialog = new PromoDialog(mContext) {
                @Override
                public DialogParams getDialogParams() {
                    return mDialogParams;
                }

                @Override
                public void onDismiss(DialogInterface dialog) {}

                @Override
                public void onClick(View view) {
                    if (view.getId() == R.id.button_primary) {
                        primaryCallback.notifyCalled();
                    } else if (view.getId() == R.id.button_secondary) {
                        secondaryCallback.notifyCalled();
                    }
                }
            };
            dialog.onCreate(null);

            // Measure the PromoDialogLayout so that the controls have some size.
            PromoDialogLayout promoDialogLayout =
                    (PromoDialogLayout) dialog.getWindow().getDecorView().findViewById(
                            R.id.promo_dialog_layout);
            int widthMeasureSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY);
            int heightMeasureSpec = MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY);
            promoDialogLayout.measure(widthMeasureSpec, heightMeasureSpec);

            return dialog;
        }
    }

    @Before
    public void setUp() {
        ApplicationTestUtils.setUp(InstrumentationRegistry.getTargetContext(), true);
    }

    @After
    public void tearDown() throws Exception {
        ApplicationTestUtils.tearDown(InstrumentationRegistry.getTargetContext());
    }

    @Test
    @SmallTest
    public void testBasic_Visibility() throws Exception {
        // Create a full dialog.
        DialogParams dialogParams = new DialogParams();
        dialogParams.drawableResource = R.drawable.search_sogou;
        dialogParams.headerStringResource = R.string.search_with_sogou;
        dialogParams.subheaderStringResource = R.string.sogou_explanation;
        dialogParams.primaryButtonStringResource = R.string.ok;
        dialogParams.secondaryButtonStringResource = R.string.cancel;
        dialogParams.footerStringResource = R.string.learn_more;
        checkDialogControlVisibility(dialogParams);

        // Create a minimal dialog.
        dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.search_with_sogou;
        dialogParams.primaryButtonStringResource = R.string.ok;
        checkDialogControlVisibility(dialogParams);
    }

    /** Confirm that PromoDialogs are constructed with all the elements expected. */
    private void checkDialogControlVisibility(final DialogParams dialogParams) throws Exception {
        PromoDialogWrapper wrapper = new PromoDialogWrapper(dialogParams);
        PromoDialog dialog = wrapper.dialog;
        PromoDialogLayout promoDialogLayout =
                (PromoDialogLayout) dialog.getWindow().getDecorView().findViewById(
                        R.id.promo_dialog_layout);

        View illustration = promoDialogLayout.findViewById(R.id.illustration);
        View header = promoDialogLayout.findViewById(R.id.header);
        View subheader = promoDialogLayout.findViewById(R.id.subheader);
        View primary = promoDialogLayout.findViewById(R.id.button_primary);
        View secondary = promoDialogLayout.findViewById(R.id.button_secondary);
        View footer = promoDialogLayout.findViewById(R.id.footer);

        // Any controls not specified by the DialogParams won't exist.
        checkControlVisibility(illustration, dialogParams.drawableResource != 0);
        checkControlVisibility(header, dialogParams.headerStringResource != 0);
        checkControlVisibility(subheader, dialogParams.subheaderStringResource != 0);
        checkControlVisibility(primary, dialogParams.primaryButtonStringResource != 0);
        checkControlVisibility(secondary, dialogParams.secondaryButtonStringResource != 0);
        checkControlVisibility(footer, dialogParams.footerStringResource != 0);
    }

    /** Check if a control should be visible. */
    private void checkControlVisibility(View view, boolean shouldBeVisible) {
        Assert.assertEquals(shouldBeVisible, view != null);
        if (view != null) {
            Assert.assertTrue(view.getMeasuredWidth() > 0);
            Assert.assertTrue(view.getMeasuredHeight() > 0);
        }
    }

    @Test
    @SmallTest
    public void testBasic_Orientation() throws Exception {
        DialogParams dialogParams = new DialogParams();
        dialogParams.drawableResource = R.drawable.search_sogou;
        dialogParams.headerStringResource = R.string.search_with_sogou;
        dialogParams.subheaderStringResource = R.string.sogou_explanation;
        dialogParams.primaryButtonStringResource = R.string.ok;
        dialogParams.secondaryButtonStringResource = R.string.cancel;
        dialogParams.footerStringResource = R.string.learn_more;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(dialogParams);
        final PromoDialogLayout promoDialogLayout =
                (PromoDialogLayout) wrapper.dialog.getWindow().getDecorView().findViewById(
                        R.id.promo_dialog_layout);
        LinearLayout flippableLayout =
                (LinearLayout) promoDialogLayout.findViewById(R.id.full_promo_content);

        // Tall screen should keep the illustration above everything else.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                int widthMeasureSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY);
                int heightMeasureSpec = MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY);
                promoDialogLayout.measure(widthMeasureSpec, heightMeasureSpec);
            }
        });
        Assert.assertEquals(LinearLayout.VERTICAL, flippableLayout.getOrientation());

        // Wide screen should move the image left.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                int widthMeasureSpec = MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY);
                int heightMeasureSpec = MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY);
                promoDialogLayout.measure(widthMeasureSpec, heightMeasureSpec);
            }
        });
        Assert.assertEquals(LinearLayout.HORIZONTAL, flippableLayout.getOrientation());
    }

    @Test
    @SmallTest
    public void testBasic_ButtonClicks() throws Exception {
        DialogParams dialogParams = new DialogParams();
        dialogParams.headerStringResource = R.string.search_with_sogou;
        dialogParams.primaryButtonStringResource = R.string.ok;
        dialogParams.secondaryButtonStringResource = R.string.cancel;

        PromoDialogWrapper wrapper = new PromoDialogWrapper(dialogParams);
        final PromoDialogLayout promoDialogLayout =
                (PromoDialogLayout) wrapper.dialog.getWindow().getDecorView().findViewById(
                        R.id.promo_dialog_layout);

        // Nothing should have been clicked yet.
        Assert.assertEquals(0, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(0, wrapper.secondaryCallback.getCallCount());

        // Only the primary button should register a click.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                promoDialogLayout.findViewById(R.id.button_primary).performClick();
            }
        });
        Assert.assertEquals(1, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(0, wrapper.secondaryCallback.getCallCount());

        // Only the secondary button should register a click.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                promoDialogLayout.findViewById(R.id.button_secondary).performClick();
            }
        });
        Assert.assertEquals(1, wrapper.primaryCallback.getCallCount());
        Assert.assertEquals(1, wrapper.secondaryCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testBasic_HeaderBehavior() throws Exception {
        // Without an illustration, the header View becomes locked to the top of the layout.
        {
            DialogParams dialogParams = new DialogParams();
            dialogParams.headerStringResource = R.string.search_with_sogou;
            dialogParams.primaryButtonStringResource = R.string.ok;

            PromoDialogWrapper wrapper = new PromoDialogWrapper(dialogParams);
            PromoDialogLayout promoDialogLayout =
                    (PromoDialogLayout) wrapper.dialog.getWindow().getDecorView().findViewById(
                            R.id.promo_dialog_layout);

            View header = promoDialogLayout.findViewById(R.id.header);
            MarginLayoutParams headerParams = (MarginLayoutParams) header.getLayoutParams();
            Assert.assertEquals(promoDialogLayout.getChildAt(0), header);
            Assert.assertNotEquals(0, ApiCompatibilityUtils.getMarginStart(headerParams));
            Assert.assertNotEquals(0, ApiCompatibilityUtils.getMarginEnd(headerParams));
        }

        // With an illustration, the header View is part of the scrollable content.
        {
            DialogParams dialogParams = new DialogParams();
            dialogParams.drawableResource = R.drawable.search_sogou;
            dialogParams.headerStringResource = R.string.search_with_sogou;
            dialogParams.primaryButtonStringResource = R.string.ok;

            PromoDialogWrapper wrapper = new PromoDialogWrapper(dialogParams);
            PromoDialogLayout promoDialogLayout =
                    (PromoDialogLayout) wrapper.dialog.getWindow().getDecorView().findViewById(
                            R.id.promo_dialog_layout);
            ViewGroup scrollableLayout =
                    (ViewGroup) promoDialogLayout.findViewById(R.id.scrollable_promo_content);

            View header = promoDialogLayout.findViewById(R.id.header);
            MarginLayoutParams headerParams = (MarginLayoutParams) header.getLayoutParams();
            Assert.assertEquals(scrollableLayout.getChildAt(0), header);
            Assert.assertEquals(0, ApiCompatibilityUtils.getMarginStart(headerParams));
            Assert.assertEquals(0, ApiCompatibilityUtils.getMarginEnd(headerParams));
        }
    }
}
