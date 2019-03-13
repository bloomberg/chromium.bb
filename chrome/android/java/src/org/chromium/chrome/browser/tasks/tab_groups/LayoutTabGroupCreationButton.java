// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.TextPaint;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.resources.dynamics.BitmapDynamicResource;

/**
 * This class is used only when TabGroup feature is enabled. This class is responsible for creating
 * the "Create group" text bitmap for TabSwitcher, and it detects and handles the click event.
 */
public class LayoutTabGroupCreationButton {
    private int mFocusedTabId;
    private final CompositorButton mCreateGroupButton;

    public LayoutTabGroupCreationButton(
            Context context, BitmapDynamicResource resource, TabModelSelector tabModelSelector) {
        float pxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        Bitmap buttonBitmap = createBitmapForButton(context);
        resource.setBitmap(buttonBitmap);

        CompositorButton.CompositorOnClickHandler buttonClickHandler = (time) -> {
            TabModel currentTabModel = tabModelSelector.getCurrentModel();
            Tab parentTab = TabModelUtils.getTabById(currentTabModel, mFocusedTabId);
            currentTabModel.commitAllTabClosures();
            tabModelSelector.openNewTab(new LoadUrlParams(UrlConstants.NTP_URL),
                    TabLaunchType.FROM_CHROME_UI, parentTab,
                    tabModelSelector.isIncognitoSelected());
        };

        mCreateGroupButton = new CompositorButton(context, buttonBitmap.getWidth() * pxToDp,
                buttonBitmap.getHeight() * pxToDp, buttonClickHandler);

        mCreateGroupButton.setResources(
                resource.getResId(), resource.getResId(), resource.getResId(), resource.getResId());
        mCreateGroupButton.setClickSlop(
                context.getResources().getDimension(R.dimen.compositor_button_slop) * pxToDp);
    }

    private Bitmap createBitmapForButton(Context context) {
        Resources res = context.getResources();
        String text = context.getResources().getString(R.string.tabswitcher_create_group);
        float textSize = res.getDimension(R.dimen.compositor_tab_title_text_size);

        TextPaint textPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        textPaint.setColor(ApiCompatibilityUtils.getColor(res, R.color.modern_blue_600));
        textPaint.setTextAlign(Paint.Align.LEFT);
        textPaint.setTextSize(textSize);
        textPaint.setFakeBoldText(true);
        textPaint.density = res.getDisplayMetrics().density;

        int width = (int) textPaint.measureText(text);
        Paint.FontMetrics textFontMetrics = textPaint.getFontMetrics();
        int height = (int) Math.ceil(textFontMetrics.bottom - textFontMetrics.top);

        Bitmap createGroupTextButtonBitmap =
                Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(createGroupTextButtonBitmap);
        canvas.drawText(text, 0, -textFontMetrics.top, textPaint);

        return createGroupTextButtonBitmap;
    }

    public CompositorButton getCreateGroupButton() {
        return mCreateGroupButton;
    }

    /**
     * @return The created tab group text bitmap dynamic resource id.
     */
    public int getButtonResourceId() {
        return mCreateGroupButton.getResourceId();
    }

    /**
     * This method updates the Layout.
     * @param tab The current focused tab.
     * @param layoutTabs A list of LayoutTab.
     * @param ableToCreateGroup Whether create group is allowed.
     */
    public void updateLayout(Tab tab, LayoutTab[] layoutTabs, boolean ableToCreateGroup) {
        if (tab == null || layoutTabs == null) return;

        int centerIndex = 0;
        for (int i = 0; i < layoutTabs.length; ++i) {
            if (layoutTabs[i].getId() == tab.getId()) {
                centerIndex = i;
                break;
            }
        }

        final int verticalOffset = 25;
        float x = (layoutTabs[centerIndex].getX()
                          + layoutTabs[centerIndex].getFinalContentWidth() / 2)
                - mCreateGroupButton.getWidth() / 2;
        float y = (layoutTabs[centerIndex].getY() + layoutTabs[centerIndex].getFinalContentHeight()
                + verticalOffset);

        mCreateGroupButton.setX(x);
        mCreateGroupButton.setY(y);
        mCreateGroupButton.setVisible(ableToCreateGroup);
        mFocusedTabId = tab.getId();
    }
}
