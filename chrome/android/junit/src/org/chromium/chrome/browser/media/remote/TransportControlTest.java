// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


package org.chromium.chrome.browser.media.remote;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

/**
 * Robolectric tests of TransportControl class
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TransportControlTest {

    TransportControl mTransportControl;

    @Before
    public void setUp() {
        mTransportControl = Mockito.mock(TransportControl.class, Mockito.CALLS_REAL_METHODS);
    }

    /**
     * Test method for {@link TransportControl#setScreenName} .
     */
    @Test
    @Feature("MediaRemote")
    public void testSetScreenName() {
        mTransportControl.setScreenName("Name1");
        assertThat("getScreenName should return the last screen name set",
                mTransportControl.getScreenName(), equalTo("Name1"));
        // Check that onScreenNameChanged is called precisely once
        verify(mTransportControl).onScreenNameChanged();
        // Try setting the same screen name again
        mTransportControl.setScreenName("Name1");
        // There should still have only been one call to onScreenNameChanged
        verify(mTransportControl).onScreenNameChanged();
        // And try a different screen name
        mTransportControl.setScreenName("Name2");
        assertThat("getScreenName should return the last screen name set",
                mTransportControl.getScreenName(), equalTo("Name2"));
        // Now there should have been two calls
        verify(mTransportControl, times(2)).onScreenNameChanged();
    }

    /**
     * Test method for {@link TransportControl#setError} .
     */
    @Test
    @Feature("MediaRemote")
    public void testSetError() {
        mTransportControl.setError("Error1");
        assertThat("getError should return the last error set", mTransportControl.getError(),
                equalTo("Error1"));
        // Check that onErrorChanged is called precisely once
        verify(mTransportControl).onErrorChanged();
        // Try setting the same error again
        mTransportControl.setError("Error1");
        // There should still have only been one call to onErrorChanged
        verify(mTransportControl).onErrorChanged();
        // And try a different error message
        mTransportControl.setError("Error2");
        assertThat("getError should return the last screen name set", mTransportControl.getError(),
                equalTo("Error2"));
        // Now there should have been two calls
        verify(mTransportControl, times(2)).onErrorChanged();
        // Now try with an empty string
        mTransportControl.setError("");
        assertThat("empty error string should set error to null", mTransportControl.getError(),
                nullValue());
        verify(mTransportControl, times(3)).onErrorChanged();
        // Try setting the empty string a second time, should not call onErrorChanged
        mTransportControl.setError("");
        verify(mTransportControl, times(3)).onErrorChanged();
        // Also try null, which should be equivalent to an empty string
        mTransportControl.setError(null);
        verify(mTransportControl, times(3)).onErrorChanged();
    }

    /**
     * Test method for {@link TransportControl#setVideoInfo} .
     */
    @Test
    @Feature("MediaRemote")
    public void testSetVideoInfo() {
        RemoteVideoInfo videoInfo1 = new RemoteVideoInfo("Title1", 10,
                RemoteVideoInfo.PlayerState.STOPPED, 5, "Error1");
        RemoteVideoInfo videoInfo2 = new RemoteVideoInfo("Title1", 10,
                RemoteVideoInfo.PlayerState.STOPPED, 5, "Error1");
        RemoteVideoInfo videoInfo3 = new RemoteVideoInfo("Title3", 10,
                RemoteVideoInfo.PlayerState.STOPPED, 5, "Error1");
        mTransportControl.setVideoInfo(videoInfo1);
        assertThat("getVideoInfo should return the videoInfo that was set",
                mTransportControl.getVideoInfo(), equalTo(videoInfo1));
        // Check that onVideoInfoChanged is called precisely once
        verify(mTransportControl).onVideoInfoChanged();
        // Try setting the same video info again
        mTransportControl.setVideoInfo(videoInfo1);
        // There should still have only been one call to onVideoInfoChanged
        verify(mTransportControl).onVideoInfoChanged();
        // Set the video info to a copy of the original video info
        mTransportControl.setVideoInfo(videoInfo2);
        // There should still have only been one call to onVideoInfoChanged
        verify(mTransportControl).onVideoInfoChanged();
        // And try a different video info
        mTransportControl.setVideoInfo(videoInfo3);
        assertThat("getVideoInfo should return the last videoInfo set",
                mTransportControl.getVideoInfo(), equalTo(videoInfo3));
        // Now there should have been two calls
        verify(mTransportControl, times(2)).onVideoInfoChanged();

    }

    /**
     * Test method for {@link TransportControl#setPosterBitmap} .
     */
    @Test
    @Feature("MediaRemote")
    public void testSetPosterBitmap() {
        Bitmap.Config conf = Bitmap.Config.ARGB_8888;
        int c[] = new int[] {Color.RED};
        Bitmap bmp1 = Bitmap.createBitmap(c, 1, 1, conf);
        mTransportControl.setPosterBitmap(bmp1);
        assertThat("getPosterBitmap gets the last set poster", mTransportControl.getPosterBitmap(),
                equalTo(bmp1));
        // onPosterBitmapChanged should have been called precisely once
        verify(mTransportControl).onPosterBitmapChanged();
        // Try setting the same poster again
        mTransportControl.setPosterBitmap(bmp1);
        // there should still have been precisely one call to onPosterBitmapChanged
        verify(mTransportControl).onPosterBitmapChanged();
        // TODO(aberent) Cannot test changing the bitmap, because of a bug in Robolectric bitmap
        // comparison (https://github.com/robolectric/robolectric/issues/1684).
    }

    /**
     * Test method for {@link TransportControl#addListener} Also tests getListener
     */
    @Test
    @Feature("MediaRemote")
    public void testAddListener() {
        assertThat("The listener set is empty", mTransportControl.getListeners().size(),
                equalTo(0));
        TransportControl.Listener listener1 = Mockito.mock(TransportControl.Listener.class);
        mTransportControl.addListener(listener1);
        assertThat("The listener set contains one item", mTransportControl.getListeners().size(),
                equalTo(1));
        // TODO(aberent): Change to CoreMatchers.hasItems when Hamcrest has been upgraded to a more
        // modern version
        assertThat("An added listener is returned",
                mTransportControl.getListeners().contains(listener1), equalTo(true));
        // Add a second listener
        TransportControl.Listener listener2 = Mockito.mock(TransportControl.Listener.class);
        mTransportControl.addListener(listener2);
        assertThat("The listener set contains two items", mTransportControl.getListeners().size(),
                equalTo(2));
        // TODO(aberent): Change to CoreMatchers.hasItems when Hamcrest has been upgraded to a more
        // modern version
        assertThat("Adding a listener does not remove the old listener",
                mTransportControl.getListeners().contains(listener1), equalTo(true));
        assertThat("The second added listener is returned",
                mTransportControl.getListeners().contains(listener2), equalTo(true));
    }

    /**
     * Test method for {@link TransportControl#removeListener} .
     */
    @Test
    @Feature("MediaRemote")
    public void testRemoveListener() {
        TransportControl.Listener listener1 = Mockito.mock(TransportControl.Listener.class);
        mTransportControl.addListener(listener1);
        TransportControl.Listener listener2 = Mockito.mock(TransportControl.Listener.class);
        mTransportControl.addListener(listener2);
        mTransportControl.removeListener(listener1);
        assertThat("The listener set contains one item", mTransportControl.getListeners().size(),
                equalTo(1));
        assertThat("The removed listner is no longer in the list",
                mTransportControl.getListeners().contains(listener1), equalTo(false));
    }

}
