// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.protocol.AutofillProfileSpecifics;
import org.chromium.sync.protocol.AutofillSpecifics;
import org.chromium.sync.protocol.EntitySpecifics;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Test suite for the autofill sync data type.
 */
public class AutofillTest extends SyncTestBase {
    private static final String TAG = "AutofillTest";

    private static final String AUTOFILL_TYPE = "Autofill";

    private static final String STREET = "1600 Amphitheatre Pkwy";
    private static final String CITY = "Mountain View";
    private static final String STATE = "CA";
    private static final String ZIP = "94043";

    // A container to store autofill information for data verification.
    private static class Autofill {
        public final String id;
        public final String street;
        public final String city;
        public final String state;
        public final String zip;

        public Autofill(String id, String street, String city, String state, String zip) {
            this.id = id;
            this.street = street;
            this.city = city;
            this.state = state;
            this.zip = zip;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setupTestAccountAndSignInToSync(CLIENT_ID);
        // Make sure the initial state is clean.
        assertClientAutofillCount(0);
        assertServerAutofillCountWithName(0, STREET);
    }

    // Test syncing a autofill from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofill() throws Exception {
        addServerAutofillData(STREET, CITY, STATE, ZIP);
        assertServerAutofillCountWithName(1, STREET);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        // Verify data synced to client.
        List<Autofill> autofills = getClientAutofillProfiles();
        assertEquals("Only the injected autofill should exist on the client.",
                1, autofills.size());
        Autofill autofill = autofills.get(0);
        assertEquals("The wrong street was found for the address.", STREET, autofill.street);
        assertEquals("The wrong city was found for the autofill.", CITY, autofill.city);
        assertEquals("The wrong state was found for the autofill.", STATE, autofill.state);
        assertEquals("The wrong zip was found for the autofill.", ZIP, autofill.zip);
    }

    // Test syncing a autofill deletion from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedAutofill() throws Exception {
        // Add the entity to test deleting.
        addServerAutofillData(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertServerAutofillCountWithName(1, STREET);
        assertClientAutofillCount(1);

        // Delete on server, sync, and verify deleted locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        mFakeServerHelper.deleteEntity(autofill.id);
        waitForServerAutofillCountWithName(0, STREET);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForClientAutofillCount(0);
    }

    // Test that autofill entries don't get synced if the data type is disabled.
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoDownloadAutofill() throws Exception {
        disableDataType(ModelType.AUTOFILL);
        addServerAutofillData(STREET, CITY, STATE, ZIP);
        assertServerAutofillCountWithName(1, STREET);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertClientAutofillCount(0);
    }

    // TODO(maxbogue): Switch to using specifics.autofill_profile instead.
    private void addServerAutofillData(String street, String city, String state, String zip) {
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.autofill = new AutofillSpecifics();
        AutofillProfileSpecifics profile = new AutofillProfileSpecifics();
        profile.addressHomeLine1 = street;
        profile.addressHomeCity = city;
        profile.addressHomeState = state;
        profile.addressHomeZip = zip;
        specifics.autofill.profile = profile;
        mFakeServerHelper.injectUniqueClientEntity(street /* name */, specifics);
    }

    private List<Autofill> getClientAutofillProfiles() throws JSONException {
        List<Pair<String, JSONObject>> entities = SyncTestUtil.getLocalData(
                mContext, AUTOFILL_TYPE);
        List<Autofill> autofills = new ArrayList<Autofill>(entities.size());
        for (Pair<String, JSONObject> entity : entities) {
            String id = entity.first;
            JSONObject profile = entity.second.getJSONObject("profile");
            String street = profile.getString("address_home_line1");
            String city = profile.getString("address_home_city");
            String state = profile.getString("address_home_state");
            String zip = profile.getString("address_home_zip");
            autofills.add(new Autofill(id, street, city, state, zip));
        }
        return autofills;
    }

    private void assertClientAutofillCount(int count) throws JSONException {
        assertEquals("There should be " + count + " local autofill entities.",
                count, SyncTestUtil.getLocalData(mContext, AUTOFILL_TYPE).size());
    }

    private void assertServerAutofillCountWithName(int count, String name) {
        assertTrue("Expected " + count + " server autofills with name " + name + ".",
                mFakeServerHelper.verifyEntityCountByTypeAndName(
                        count, ModelType.AUTOFILL, name));
    }

    private void waitForClientAutofillCount(final int count) throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return SyncTestUtil.getLocalData(mContext, AUTOFILL_TYPE).size() == count;
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected " + count + " local autofill entities.", success);
    }

    private void waitForServerAutofillCountWithName(final int count, final String name)
            throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return mFakeServerHelper.verifyEntityCountByTypeAndName(
                            count, ModelType.AUTOFILL, name);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected " + count + " server autofills with name " + name + ".", success);
    }
}
