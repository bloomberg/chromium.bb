// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.ntp;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Chrome Menu (NTP 3-dot menu) Page Controller.
 */
public class ChromeMenu extends PageController {
    private static final IUi2Locator LOCATOR_CHROME_MENU_BOX =
            Ui2Locators.withResEntries(R.id.app_menu_list);
    private static final IUi2Locator LOCATOR_CHROME_MENU = Ui2Locators.withPath(
            LOCATOR_CHROME_MENU_BOX, Ui2Locators.withResEntries(R.id.button_five));

    private static final IUi2Locator LOCATOR_NEW_TAB =
            Ui2Locators.withResEntriesByIndex(0, R.id.menu_item_text);
    private static final IUi2Locator LOCATOR_NEW_INCOGNITO_TAB =
            Ui2Locators.withResEntriesByIndex(1, R.id.menu_item_text);

    private static final ChromeMenu sInstance = new ChromeMenu();
    private ChromeMenu() {}
    static public ChromeMenu getInstance() {
        return sInstance;
    }

    @Override
    public ChromeMenu verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_CHROME_MENU);
        return this;
    }

    public NewTabPageController openNewTab() {
        mUtils.click(LOCATOR_NEW_TAB);
        return NewTabPageController.getInstance().verifyActive();
    }

    public IncognitoNewTabPageController openNewIncognitoTab() {
        mUtils.click(LOCATOR_NEW_INCOGNITO_TAB);
        return IncognitoNewTabPageController.getInstance().verifyActive();
    }

    public NewTabPageController dismiss() {
        mUtils.clickOutsideOf(LOCATOR_CHROME_MENU_BOX);
        return NewTabPageController.getInstance().verifyActive();
    }
}
