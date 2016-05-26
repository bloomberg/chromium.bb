// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Tests for the ActivityAssigner class.
 */
public class ActivityAssignerTest extends InstrumentationTestCase {
    private static final String BASE_WEBAPP_ID = "BASE_WEBAPP_ID_";

    private AdvancedMockContext mContext;
    private HashMap<String, Object>[] mPreferences;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        RecordHistogram.disableForTests();
        mContext = new AdvancedMockContext(ContextUtils.getApplicationContext());
        mPreferences = new HashMap[ActivityAssigner.ACTIVITY_TYPE_COUNT];
        for (int i = 0; i < ActivityAssigner.ACTIVITY_TYPE_COUNT; ++i) {
            mPreferences[i] = new HashMap<String, Object>();
            mContext.addSharedPreferences(ActivityAssigner.PREF_PACKAGE[i], mPreferences[i]);
        }
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testEntriesCreated() {
        String webappId = BASE_WEBAPP_ID;
        ActivityAssigner assigner = ActivityAssigner.instance(webappId);

        // Make sure that no webapps have been assigned to any Activities for a fresh install.
        checkState(assigner);
        List<ActivityAssigner.ActivityEntry> entries = assigner.getEntries();
        assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, entries.size());
        for (ActivityAssigner.ActivityEntry entry : entries) {
            assertEquals(null, entry.mWebappId);
        }
    }

    /**
     * Make sure invalid entries get culled & that we still have the correct number of unique
     * Activity indices available.
     */
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testEntriesDownsized() {
        // Store preferences indicating that more Activities existed previously than there are now.
        int numSavedEntries = ActivityAssigner.NUM_WEBAPP_ACTIVITIES + 1;
        String webappId = BASE_WEBAPP_ID;
        int index = ActivityAssigner.getIndex(webappId);
        createPreferences(numSavedEntries, index);

        ActivityAssigner assigner = ActivityAssigner.instance(webappId);
        assertEquals(index, assigner.getActivityTypeIndex());
        checkState(assigner);
    }

    /**
     * Make sure we recover from corrupted stored preferences.
     */
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testCorruptedPreferences() {
        String wrongVariableType = "omgwtfbbq";
        String webappId = BASE_WEBAPP_ID;
        int index = ActivityAssigner.getIndex(BASE_WEBAPP_ID);
        mPreferences[index].clear();
        mPreferences[index].put(ActivityAssigner.PREF_NUM_SAVED_ENTRIES[index], wrongVariableType);

        ActivityAssigner assigner = ActivityAssigner.instance(webappId);
        assertEquals(index, assigner.getActivityTypeIndex());
        checkState(assigner);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testAssignment() {
        String webappId = BASE_WEBAPP_ID;
        ActivityAssigner assigner = ActivityAssigner.instance(webappId);
        int index = assigner.getActivityTypeIndex();

        checkState(assigner);

        // Assign all of the Activities to webapps.
        // Go backwards to make sure ordering doesn't matter.
        Map<String, Integer> testMap = new HashMap<String, Integer>();
        for (int i = ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1; i >= 0; --i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int activityIndex = assigner.assign(currentWebappId);
            testMap.put(currentWebappId, activityIndex);
        }
        assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, testMap.size());

        // Make sure that passing in the same webapp ID gives back the same Activity.
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES; ++i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int actualIndex = assigner.assign(currentWebappId);
            int expectedIndex = testMap.get(currentWebappId);
            assertEquals(expectedIndex, actualIndex);
        }

        // Access all but the last one to ensure that the last Activity is recycled.
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1; ++i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int activityIndex = testMap.get(currentWebappId);
            assigner.markActivityUsed(activityIndex, currentWebappId);
        }

        // Make sure that the least recently used Activity is repurposed when we run out.
        String overflowWebappId = "OVERFLOW_ID";
        assertEquals(index, ActivityAssigner.getIndex(overflowWebappId));
        int overflowActivityIndex = assigner.assign(overflowWebappId);

        String lastAssignedWebappId = BASE_WEBAPP_ID + (ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1);
        int lastAssignedCurrentActivityIndex = assigner.assign(lastAssignedWebappId);
        int lastAssignedPreviousActivityIndex = testMap.get(lastAssignedWebappId);

        assertEquals("Overflow webapp did not steal the Activity from the other webapp",
                lastAssignedPreviousActivityIndex, overflowActivityIndex);
        assertNotSame("Webapp did not get reassigned to a new Activity.",
                lastAssignedPreviousActivityIndex, lastAssignedCurrentActivityIndex);

        checkState(assigner);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"WebApk"})
    public void testGetIndex() {
        String webappId = BASE_WEBAPP_ID;
        assertEquals(ActivityAssigner.WEBAPP_ACTIVITY_INDEX, ActivityAssigner.getIndex(webappId));

        String webApkId = WebApkConstants.WEBAPK_ID_PREFIX + "id";
        assertEquals(ActivityAssigner.WEBAPK_ACTIVITY_INDEX, ActivityAssigner.getIndex(webApkId));
    }

    /** Saves state indicating that a number of WebappActivities have already been saved out. */
    private void createPreferences(int numSavedEntries, int activityTypeIndex) {
        mPreferences[activityTypeIndex].clear();
        mPreferences[activityTypeIndex].put(
                ActivityAssigner.PREF_NUM_SAVED_ENTRIES[activityTypeIndex], numSavedEntries);
        for (int i = 0; i < numSavedEntries; ++i) {
            String activityIndexKey =
                    ActivityAssigner.PREF_ACTIVITY_INDEX[activityTypeIndex] + i;
            mPreferences[activityTypeIndex].put(activityIndexKey, i);

            String webappIdKey = ActivityAssigner.PREF_WEBAPP_ID[activityTypeIndex] + i;
            String webappIdValue = BASE_WEBAPP_ID + i;
            mPreferences[activityTypeIndex].put(webappIdKey, webappIdValue);
        }
    }

    /** Checks the saved state to make sure it makes sense. */
    private void checkState(ActivityAssigner assigner) {
        List<ActivityAssigner.ActivityEntry> entries = assigner.getEntries();
        int activityTypeIndex = assigner.getActivityTypeIndex();

        // Confirm that the right number of entries in memory and in the preferences.
        assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, entries.size());
        assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES,
                (int) (Integer) mPreferences[activityTypeIndex].get(
                        ActivityAssigner.PREF_NUM_SAVED_ENTRIES[activityTypeIndex]));

        // Confirm that the Activity indices go from 0 to NUM_WEBAPP_ACTIVITIES - 1.
        HashSet<Integer> assignedActivities = new HashSet<Integer>();
        for (ActivityAssigner.ActivityEntry entry : entries) {
            assignedActivities.add(entry.mActivityIndex);
        }
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES; ++i) {
            assertTrue(assignedActivities.contains(i));
        }
    }
}
