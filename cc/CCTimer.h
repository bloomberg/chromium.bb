// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTimer_h
#define CCTimer_h


namespace cc {

class CCThread;
class CCTimerTask;

class CCTimerClient {
public:
    virtual ~CCTimerClient() { }

    virtual void onTimerFired() = 0;
};

class CCTimer {
public:
    CCTimer(CCThread*, CCTimerClient*);
    ~CCTimer();

    // If a previous task is pending, it will be replaced with the new one.
    void startOneShot(double intervalSeconds);
    void stop();

    bool isActive() const { return m_task; }

private:
    friend class CCTimerTask;

    CCTimerClient* m_client;
    CCThread* m_thread;
    CCTimerTask* m_task; // weak pointer
};

} // namespace cc

#endif
