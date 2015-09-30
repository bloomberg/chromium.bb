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
import org.chromium.sync.ModelType;
import org.chromium.sync.protocol.AutofillProfileSpecifics;
import org.chromium.sync.protocol.EntitySpecifics;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Test suite for the autofill profile sync data type.
 */
public class AutofillTest extends SyncTestBase {
    private static final String TAG = "AutofillTest";

    private static final String AUTOFILL_TYPE = "Autofill Profiles";

    private static final String GUID = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
    private static final String ORIGIN = "https://www.chromium.org/";

    private static final String STREET = "1600 Amphitheatre Pkwy";
    private static final String CITY = "Mountain View";
    private static final String MODIFIED_CITY = "Sunnyvale";
    private static final String STATE = "CA";
    private static final String ZIP = "94043";

    // A container to store autofill profile information for data verification.
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
        setUpTestAccountAndSignInToSync();
        // Make sure the initial state is clean.
        assertClientAutofillProfileCount(0);
        assertServerAutofillProfileCountWithName(0, STREET);
    }

    // Test syncing an autofill profile from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofill() throws Exception {
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        assertServerAutofillProfileCountWithName(1, STREET);
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

    // Test syncing an autofill profile modification from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofillModification() throws Exception {
        // Add the entity to test modifying.
        EntitySpecifics specifics = addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertServerAutofillProfileCountWithName(1, STREET);
        assertClientAutofillProfileCount(1);

        // Modify on server, sync, and verify modification locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        specifics.autofillProfile.addressHomeCity = MODIFIED_CITY;
        mFakeServerHelper.modifyEntitySpecifics(autofill.id, specifics);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        Autofill modifiedAutofill = getClientAutofillProfiles().get(0);
        assertEquals("The city was not modified.", MODIFIED_CITY, modifiedAutofill.city);
    }

    // Test syncing an autofill profile deletion from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedAutofill() throws Exception {
        // Add the entity to test deleting.
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertServerAutofillProfileCountWithName(1, STREET);
        assertClientAutofillProfileCount(1);

        // Delete on server, sync, and verify deleted locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        mFakeServerHelper.deleteEntity(autofill.id);
        waitForServerAutofillProfileCountWithName(0, STREET);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForClientAutofillProfileCount(0);
    }

    // Test that autofill profiles don't get synced if the data type is disabled.
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoDownloadAutofill() throws Exception {
        // The AUTOFILL type here controls both AUTOFILL and AUTOFILL_PROFILE.
        disableDataType(ModelType.AUTOFILL);
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        assertServerAutofillProfileCountWithName(1, STREET);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertClientAutofillProfileCount(0);
    }

    private EntitySpecifics addServerAutofillProfile(
            String street, String city, String state, String zip) {
        EntitySpecifics specifics = new EntitySpecifics();
        AutofillProfileSpecifics profile = new AutofillProfileSpecifics();
        profile.guid = GUID;
        profile.origin = ORIGIN;
        profile.addressHomeLine1 = street;
        profile.addressHomeCity = city;
        profile.addressHomeState = state;
        profile.addressHomeZip = zip;
        specifics.autofillProfile = profile;
        mFakeServerHelper.injectUniqueClientEntity(street /* name */, specifics);
        return specifics;
    }

    private List<Autofill> getClientAutofillProfiles() throws JSONException {
        List<Pair<String, JSONObject>> entities = SyncTestUtil.getLocalData(
                mContext, AUTOFILL_TYPE);
        List<Autofill> autofills = new ArrayList<Autofill>(entities.size());
        for (Pair<String, JSONObject> entity : entities) {
            String id = entity.first;
            JSONObject profile = entity.second;
            String street = profile.getString("address_home_line1");
            String city = profile.getString("address_home_city");
            String state = profile.getString("address_home_state");
            String zip = profile.getString("address_home_zip");
            autofills.add(new Autofill(id, street, city, state, zip));
        }
        return autofills;
    }

    private void assertClientAutofillProfileCount(int count) throws JSONException {
        assertEquals("There should be " + count + " local autofill profiles.",
                count, SyncTestUtil.getLocalData(mContext, AUTOFILL_TYPE).size());
    }

    private void assertServerAutofillProfileCountWithName(int count, String name) {
        assertTrue("Expected " + count + " server autofill profiles with name " + name + ".",
                mFakeServerHelper.verifyEntityCountByTypeAndName(
                        count, ModelType.AUTOFILL_PROFILE, name));
    }

    private void waitForClientAutofillProfileCount(final int count) throws InterruptedException {
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
        assertTrue("Expected " + count + " local autofill profiles.", success);
    }

    private void waitForServerAutofillProfileCountWithName(final int count, final String name)
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
        assertTrue("Expected " + count + " server autofill profiles with name " + name + ".",
                success);
    }
}
