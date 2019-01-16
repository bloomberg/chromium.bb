// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.support.annotation.Nullable;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.JNINamespace;

import java.util.LinkedList;

/**
 * Implementation of the abstract class {@link TaskRunnerImpl}. Uses AsyncTasks until
 * native APIs are available.
 */
@JNINamespace("base")
public class TaskRunnerImpl implements TaskRunner {
    @Nullable
    private final TaskTraits mTaskTraits;
    private final String mTraceEvent;
    private final @TaskRunnerType int mTaskRunnerType;
    private final Object mLock = new Object();
    protected long mNativeTaskRunnerAndroid;
    protected final Runnable mRunPreNativeTaskClosure = this::runPreNativeTask;
    private boolean mIsDestroying;

    @Nullable
    protected LinkedList<Runnable> mPreNativeTasks = new LinkedList<>();

    /**
     * @param traits The TaskTraits associated with this TaskRunnerImpl.
     */
    TaskRunnerImpl(TaskTraits traits) {
        this(traits, "TaskRunnerImpl", TaskRunnerType.BASE);
    }

    /**
     * @param traits The TaskTraits associated with this TaskRunnerImpl.
     * @param traceCategory Specifies which subclass is this instance for logging purposes.
     * @param taskRunnerType Specifies which subclass is this instance for initialising the correct
     *         native scheduler.
     */
    protected TaskRunnerImpl(
            TaskTraits traits, String traceCategory, @TaskRunnerType int taskRunnerType) {
        mTaskTraits = traits;
        mTraceEvent = traceCategory + ".PreNativeTask.run";
        mTaskRunnerType = taskRunnerType;
    }

    @Override
    public void destroy() {
        synchronized (mLock) {
            if (mNativeTaskRunnerAndroid != 0) nativeDestroy(mNativeTaskRunnerAndroid);
            mNativeTaskRunnerAndroid = 0;
            mIsDestroying = true;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            // Calling destroy as a fallback in case destroy wasn't called by the user.
            destroy();
        } finally {
            super.finalize();
        }
    }

    @Override
    public void postTask(Runnable task) {
        synchronized (mLock) {
            assert !mIsDestroying;
            if (mNativeTaskRunnerAndroid != 0) {
                nativePostTask(mNativeTaskRunnerAndroid, task);
                return;
            }
            // We don't expect a whole lot of these, if that changes consider pooling them.
            mPreNativeTasks.add(task);
            schedulePreNativeTask();
        }
    }

    /**
     * Must be overridden in subclasses, schedules a call to runPreNativeTask() at an appropriate
     * time.
     */
    protected void schedulePreNativeTask() {
        AsyncTask.THREAD_POOL_EXECUTOR.execute(mRunPreNativeTaskClosure);
    }

    /**
     * Runs a single task and returns when its finished.
     */
    protected void runPreNativeTask() {
        try (TraceEvent te = TraceEvent.scoped(mTraceEvent)) {
            Runnable task;
            synchronized (mLock) {
                if (mPreNativeTasks == null) return;
                task = mPreNativeTasks.poll();
            }
            task.run();
        }
    }

    /**
     * Instructs the TaskRunner to initialize the native TaskRunner and migrate any tasks over to
     * it.
     */
    @Override
    public void initNativeTaskRunner() {
        synchronized (mLock) {
            if (mPreNativeTasks != null) {
                mNativeTaskRunnerAndroid =
                        nativeInit(mTaskRunnerType, mTaskTraits.mPrioritySetExplicitly,
                                mTaskTraits.mPriority, mTaskTraits.mMayBlock,
                                mTaskTraits.mExtensionId, mTaskTraits.mExtensionData);
                for (Runnable task : mPreNativeTasks) {
                    nativePostTask(mNativeTaskRunnerAndroid, task);
                }
                mPreNativeTasks = null;
            }
        }
    }

    // NB due to Proguard obfuscation it's easiest to pass the traits via arguments.
    private native long nativeInit(@TaskRunnerType int taskRunnerType,
            boolean prioritySetExplicitly, int priority, boolean mayBlock, byte extensionId,
            byte[] extensionData);
    private native void nativeDestroy(long nativeTaskRunnerAndroid);
    private native void nativePostTask(long nativeTaskRunnerAndroid, Runnable task);
    protected native boolean nativeBelongsToCurrentThread(long nativeTaskRunnerAndroid);
}
