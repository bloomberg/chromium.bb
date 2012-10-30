// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCThreadTask_h
#define CCThreadTask_h

#include "cc/thread.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

template<typename T>
class ThreadTask0 : public Thread::Task {
public:
    typedef void (T::*Method)();
    typedef ThreadTask0<T> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method));
    }

private:
    ThreadTask0(T* instance, Method method)
        : Thread::Task(instance)
        , m_method(method)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)();
    }

private:
    Method m_method;
};

template<typename T, typename P1, typename MP1>
class ThreadTask1 : public Thread::Task {
public:
    typedef void (T::*Method)(MP1);
    typedef ThreadTask1<T, P1, MP1> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method, P1 parameter1)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method, parameter1));
    }

private:
    ThreadTask1(T* instance, Method method, P1 parameter1)
        : Thread::Task(instance)
        , m_method(method)
        , m_parameter1(parameter1)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)(m_parameter1);
    }

private:
    Method m_method;
    P1 m_parameter1;
};

template<typename T, typename P1, typename MP1, typename P2, typename MP2>
class ThreadTask2 : public Thread::Task {
public:
    typedef void (T::*Method)(MP1, MP2);
    typedef ThreadTask2<T, P1, MP1, P2, MP2> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method, P1 parameter1, P2 parameter2)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method, parameter1, parameter2));
    }

private:
    ThreadTask2(T* instance, Method method, P1 parameter1, P2 parameter2)
        : Thread::Task(instance)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)(m_parameter1, m_parameter2);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
};

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
class ThreadTask3 : public Thread::Task {
public:
    typedef void (T::*Method)(MP1, MP2, MP3);
    typedef ThreadTask3<T, P1, MP1, P2, MP2, P3, MP3> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method, parameter1, parameter2, parameter3));
    }

private:
    ThreadTask3(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3)
        : Thread::Task(instance)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)(m_parameter1, m_parameter2, m_parameter3);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
};


template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
class ThreadTask4 : public Thread::Task {
public:
    typedef void (T::*Method)(MP1, MP2, MP3, MP4);
    typedef ThreadTask4<T, P1, MP1, P2, MP2, P3, MP3, P4, MP4> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3, P4 parameter4)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method, parameter1, parameter2, parameter3, parameter4));
    }

private:
    ThreadTask4(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3, P4 parameter4)
        : Thread::Task(instance)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)(m_parameter1, m_parameter2, m_parameter3, m_parameter4);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
};

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
class ThreadTask5 : public Thread::Task {
public:
    typedef void (T::*Method)(MP1, MP2, MP3, MP4, MP5);
    typedef ThreadTask5<T, P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5> ThreadTaskImpl;

    static PassOwnPtr<ThreadTaskImpl> create(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3, P4 parameter4, P5 parameter5)
    {
        return adoptPtr(new ThreadTaskImpl(instance, method, parameter1, parameter2, parameter3, parameter4, parameter5));
    }

private:
    ThreadTask5(T* instance, Method method, P1 parameter1, P2 parameter2, P3 parameter3, P4 parameter4, P5 parameter5)
        : Thread::Task(instance)
        , m_method(method)
        , m_parameter1(parameter1)
        , m_parameter2(parameter2)
        , m_parameter3(parameter3)
        , m_parameter4(parameter4)
        , m_parameter5(parameter5)
    {
    }

    virtual void performTask() OVERRIDE
    {
        (*static_cast<T*>(instance()).*m_method)(m_parameter1, m_parameter2, m_parameter3, m_parameter4, m_parameter5);
    }

private:
    Method m_method;
    P1 m_parameter1;
    P2 m_parameter2;
    P3 m_parameter3;
    P4 m_parameter4;
    P5 m_parameter5;
};

template<typename T>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)());

template<typename T>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)())
{
    return ThreadTask0<T>::create(
        callee,
        method);
}

template<typename T, typename P1, typename MP1>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)(MP1),
    const P1& parameter1)
{
    return ThreadTask1<T, P1, MP1>::create(
        callee,
        method,
        parameter1);
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)(MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return ThreadTask2<T, P1, MP1, P2, MP2>::create(
        callee,
        method,
        parameter1,
        parameter2);
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)(MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return ThreadTask3<T, P1, MP1, P2, MP2, P3, MP3>::create(
        callee,
        method,
        parameter1,
        parameter2,
        parameter3);
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)(MP1, MP2, MP3, MP4),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4)
{
    return ThreadTask4<T, P1, MP1, P2, MP2, P3, MP3, P4, MP4>::create(
        callee,
        method,
        parameter1,
        parameter2,
        parameter3,
        parameter4);

}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<Thread::Task> createThreadTask(
    T* const callee,
    void (T::*method)(MP1, MP2, MP3, MP4, MP5),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3,
    const P4& parameter4,
    const P5& parameter5)
{
    return ThreadTask5<T, P1, MP1, P2, MP2, P3, MP3, P4, MP4, P5, MP5>::create(
        callee,
        method,
        parameter1,
        parameter2,
        parameter3,
        parameter4,
        parameter5);

}

}  // namespace cc

#endif // CCThreadTask_h
