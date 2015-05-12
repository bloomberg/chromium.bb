// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.content.Intent;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.ipc.invalidation.external.client.types.ObjectId;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.components.invalidation.InvalidationClientService;
import org.chromium.components.invalidation.InvalidationService;
import org.chromium.sync.AndroidSyncSettings;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationIntentProtocol;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.Set;

/**
 * Tests for {@link InvalidationService}.
 */
public class InvalidationServiceTest extends ChromeShellTestBase {
    private IntentSavingContext mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        // We don't want to use the system content resolver, so we override it.
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        delegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mContext, delegate);
    }

    @SmallTest
    @UiThreadTest
    @Feature({"Sync"})
    public void testSetRegisteredObjectIds() {
        InvalidationService service = InvalidationServiceFactory.getForTest(mContext);
        ObjectId bookmark = ModelType.BOOKMARK.toObjectId();
        service.setRegisteredObjectIds(new int[] {1, 2, bookmark.getSource()},
                                          new String[] {"a", "b", new String(bookmark.getName())});
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(InvalidationIntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate registered object ids. The bookmark object should not be registered since it is
        // a Sync type.
        assertNull(intent.getStringArrayListExtra(
                                InvalidationIntentProtocol.EXTRA_REGISTERED_TYPES));
        Set<ObjectId> objectIds = InvalidationIntentProtocol.getRegisteredObjectIds(intent);
        assertEquals(2, objectIds.size());
        assertTrue(objectIds.contains(ObjectId.newInstance(1, "a".getBytes())));
        assertTrue(objectIds.contains(ObjectId.newInstance(2, "b".getBytes())));
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        assertNotNull(intent.getComponent());
        assertEquals(InvalidationClientService.class.getName(),
                intent.getComponent().getClassName());
    }

}
