// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTimer_h
#define CCTimer_h

namespace cc {

class Thread;
class TimerTask;

class TimerClient {
public:
    virtual ~TimerClient() { }

    virtual void onTimerFired() = 0;
};

class Timer {
public:
    Timer(Thread*, TimerClient*);
    ~Timer();

    // If a previous task is pending, it will be replaced with the new one.
    void startOneShot(double intervalSeconds);
    void stop();

    bool isActive() const { return m_task; }

private:
    friend class TimerTask;

    TimerClient* m_client;
    Thread* m_thread;
    TimerTask* m_task; // weak pointer
};

}  // namespace cc

#endif
