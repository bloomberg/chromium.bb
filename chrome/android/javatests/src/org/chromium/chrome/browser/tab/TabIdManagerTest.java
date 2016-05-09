// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;

/** Tests for the TabIdManager. */
public class TabIdManagerTest extends InstrumentationTestCase {
    Context mContext;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = new AdvancedMockContext(getInstrumentation().getTargetContext());
    }

    /** Tests that IDs are stored and generated properly. */
    @UiThreadTest
    @SmallTest
    public void testBasic() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        TabIdManager manager = TabIdManager.getInstance(mContext);
        assertEquals("Wrong Tab ID was generated",
                11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        assertEquals("Wrong next Tab ID", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));
    }

    /** Tests that the max ID is updated properly. */
    @UiThreadTest
    @SmallTest
    public void testIncrementIdCounterTo() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        TabIdManager manager = TabIdManager.getInstance(mContext);
        assertEquals("Wrong Tab ID was generated",
                11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        assertEquals("Wrong next Tab ID", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));

        manager.incrementIdCounterTo(100);
        assertEquals("Didn't stay the same", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));

        manager.incrementIdCounterTo(1000000);
        assertEquals("Didn't increase", 1000000, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));
    }
}
