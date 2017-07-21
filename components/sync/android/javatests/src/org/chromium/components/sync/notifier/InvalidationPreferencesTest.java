// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync.notifier;

import android.accounts.Account;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;

import java.util.Arrays;
import java.util.Set;

/**
 * Tests for the {@link InvalidationPreferences}.
 *
 * @author dsmyers@google.com (Daniel Myers)
 */
@RetryOnFailure
public class InvalidationPreferencesTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Sync"})
    public void testReadMissingData() {
        /*
         * Test plan: read saved state from empty preferences. Verify that null is returned.
         */
        InvalidationPreferences invPreferences = new InvalidationPreferences();
        assertNull(invPreferences.getSavedSyncedAccount());
        assertNull(invPreferences.getSavedSyncedTypes());
        assertNull(invPreferences.getSavedObjectIds());
        assertNull(invPreferences.getInternalNotificationClientState());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReadWriteAndReadData() {
        /*
         * Test plan: write and read back saved state. Verify that the returned state is what
         * was written.
         */
        InvalidationPreferences invPreferences = new InvalidationPreferences();
        InvalidationPreferences.EditContext editContext = invPreferences.edit();

        // Write mix of valid and invalid types to disk to test that preferences are not
        // interpreting the data. Invalid types should never be written to disk in practice.
        Set<String> syncTypes = CollectionUtil.newHashSet("BOOKMARK", "INVALID");
        Set<ObjectId> objectIds =
                CollectionUtil.newHashSet(ObjectId.newInstance(1, "obj1".getBytes()),
                        ObjectId.newInstance(2, "obj2".getBytes()));
        Account account = new Account("test@example.com", "bogus");
        byte[] internalClientState = new byte[] {100, 101, 102};
        invPreferences.setSyncTypes(editContext, syncTypes);
        invPreferences.setObjectIds(editContext, objectIds);
        invPreferences.setAccount(editContext, account);
        invPreferences.setInternalNotificationClientState(editContext, internalClientState);

        // Nothing should yet have been written.
        assertNull(invPreferences.getSavedSyncedAccount());
        assertNull(invPreferences.getSavedSyncedTypes());
        assertNull(invPreferences.getSavedObjectIds());

        // Write the new data and verify that they are correctly read back.
        invPreferences.commit(editContext);
        assertEquals(account, invPreferences.getSavedSyncedAccount());
        assertEquals(syncTypes, invPreferences.getSavedSyncedTypes());
        assertEquals(objectIds, invPreferences.getSavedObjectIds());
        assertTrue(Arrays.equals(
                internalClientState, invPreferences.getInternalNotificationClientState()));
    }
}
