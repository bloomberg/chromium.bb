// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.test.util;

import android.os.SystemClock;

import org.chromium.chrome.browser.infobar.AnimationHelper;
import org.chromium.chrome.browser.infobar.InfoBarContainer;

import java.util.concurrent.TimeUnit;

/**
 * Allow tests to register for animation finished notifications.
 */
public class InfoBarTestAnimationListener implements InfoBarContainer.InfoBarAnimationListener {

    private static final long WAIT_MILLIS = TimeUnit.SECONDS.toMillis(5);

    private static class ConditionalWait {
        private volatile Boolean mCondition;
        private Object mLock;

        ConditionalWait() {
            mCondition = false;
            mLock = new Object();
        }

        /**
         * Waits for {@code millis} or until the value of the condition becomes true
         * if it does it resets it to false before returning so it can be reused.
         *
         * @return true if the condition becomes true before the specified {@code millis}.
         */
        public boolean waitAndExpire(long millis) throws InterruptedException {
            synchronized (mLock) {
                while (!mCondition && millis > 0) {
                    long start = SystemClock.elapsedRealtime();
                    mLock.wait(millis);
                    millis -= (SystemClock.elapsedRealtime() - start);
                }
                boolean result = mCondition;
                mCondition = false;
                return result;
            }
        }

        public void set(boolean value) {
            synchronized (mLock) {
                mCondition = value;
                if (value) {
                    mLock.notify();
                }
            }
        }
     }

    private ConditionalWait mAddAnimationFinished;
    private ConditionalWait mSwapAnimationFinished;
    private ConditionalWait mRemoveAnimationFinished;


    public InfoBarTestAnimationListener() {
        mAddAnimationFinished = new ConditionalWait();
        mSwapAnimationFinished = new ConditionalWait();
        mRemoveAnimationFinished = new ConditionalWait();
    }

    @Override
    public void notifyAnimationFinished(int animationType) {
        switch(animationType) {
            case AnimationHelper.ANIMATION_TYPE_SHOW:
                mAddAnimationFinished.set(true);
                break;
            case AnimationHelper.ANIMATION_TYPE_SWAP:
                mSwapAnimationFinished.set(true);
                break;
            case AnimationHelper.ANIMATION_TYPE_HIDE:
                mRemoveAnimationFinished.set(true);
                break;
            default:
               throw new UnsupportedOperationException(
                       "Animation finished for unknown type " + animationType);
        }
    }

    public boolean addInfoBarAnimationFinished() throws InterruptedException {
        return mAddAnimationFinished.waitAndExpire(WAIT_MILLIS);
    }

    public boolean swapInfoBarAnimationFinished() throws InterruptedException {
        return mSwapAnimationFinished.waitAndExpire(WAIT_MILLIS);
    }

    public boolean removeInfoBarAnimationFinished() throws InterruptedException {
        return mRemoveAnimationFinished.waitAndExpire(WAIT_MILLIS);
    }
}
