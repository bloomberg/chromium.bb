// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Fragment;

/**
 * A first run page shown in the First Run ViewPager.
 */
public class FirstRunPage extends Fragment {
    /**
     * @return Whether this page should be skipped on the FRE creation.
     */
    public boolean shouldSkipPageOnCreate() {
        return false;
    }

    /**
     * @return Whether the back button press was handled by this page.
     */
    public boolean interceptBackPressed() {
        return false;
    }

    /**
     * @return The interface to the host.
     */
    protected FirstRunPageDelegate getPageDelegate() {
        return (FirstRunPageDelegate) getActivity();
    }

    /**
     * Advances to the next FRE page.
     */
    protected void advanceToNextPage() {
        getPageDelegate().advanceToNextPage();
    }

    /**
     * Notifies this page that native has been initialized.
     */
    protected void onNativeInitialized() {}
}
