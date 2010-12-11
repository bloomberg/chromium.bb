// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

COMPILE_ASSERT(DefaultSingletonTraits<int>::kRegisterAtExit == true, a);

template<typename Type>
struct LockTrait : public DefaultSingletonTraits<Type> {
};

typedef void (*CallbackFunc)();

class IntSingleton {
 public:
  class DummyDifferentiatingClass {
  };

  struct Init5Trait : public DefaultSingletonTraits<IntSingleton> {
    static IntSingleton* New() {
      IntSingleton* instance = new IntSingleton();
      instance->value_ = 5;
      return instance;
    }
  };

  static IntSingleton* GetInstance() {
    return Singleton<IntSingleton>::get();
  }
  static IntSingleton* GetInstanceWithDefaultTraits() {
    return Singleton<IntSingleton,
                     DefaultSingletonTraits<IntSingleton> >::get();
  }
  static IntSingleton* GetInstanceWithDifferentiatingClass() {
    return Singleton<IntSingleton,
                     DefaultSingletonTraits<IntSingleton>,
                     DummyDifferentiatingClass>::get();
  }
  static IntSingleton* GetInstanceWithLockTrait() {
    return Singleton<IntSingleton, LockTrait<IntSingleton> >::get();
  }
  static IntSingleton* GetInstanceWithInit5Trait() {
    return Singleton<IntSingleton, Init5Trait>::get();
  }

  int value_;
};

int* SingletonInt1() {
  return &IntSingleton::GetInstance()->value_;
}

int* SingletonInt2() {
  // Force to use a different singleton than SingletonInt1.
  return &IntSingleton::GetInstanceWithDefaultTraits()->value_;
}

int* SingletonInt3() {
  // Force to use a different singleton than SingletonInt1 and SingletonInt2.
  return &IntSingleton::GetInstanceWithDifferentiatingClass()->value_;
}

int* SingletonInt4() {
  return &IntSingleton::GetInstanceWithLockTrait()->value_;
}

int* SingletonInt5() {
  return &IntSingleton::GetInstanceWithInit5Trait()->value_;
}

class CallbackSingleton {
 public:
  struct CallbackTrait : public DefaultSingletonTraits<CallbackSingleton> {
    static void Delete(CallbackSingleton* instance) {
      if (instance->callback_)
        (instance->callback_)();
      DefaultSingletonTraits<CallbackSingleton>::Delete(instance);
    }
  };

  struct NoLeakTrait : public CallbackTrait {
  };

  struct LeakTrait : public CallbackTrait {
    static const bool kRegisterAtExit = false;
  };

  CallbackSingleton() : callback_(NULL) {
  }

  static CallbackSingleton* GetInstanceWithNoLeakTrait() {
    return Singleton<CallbackSingleton, NoLeakTrait>::get();
  }
  static CallbackSingleton* GetInstanceWithLeakTrait() {
    return Singleton<CallbackSingleton, LeakTrait>::get();
  }
  static CallbackSingleton* GetInstanceWithStaticTrait();

  CallbackFunc callback_;
};

struct StaticCallbackTrait
    : public StaticMemorySingletonTraits<CallbackSingleton> {
  static void Delete(CallbackSingleton* instance) {
    if (instance->callback_)
      (instance->callback_)();
    StaticMemorySingletonTraits<CallbackSingleton>::Delete(instance);
  }
};

CallbackSingleton* CallbackSingleton::GetInstanceWithStaticTrait() {
  return Singleton<CallbackSingleton, StaticCallbackTrait>::get();
}

void SingletonNoLeak(CallbackFunc CallOnQuit) {
  CallbackSingleton::GetInstanceWithNoLeakTrait()->callback_ = CallOnQuit;
}

void SingletonLeak(CallbackFunc CallOnQuit) {
  CallbackSingleton::GetInstanceWithLeakTrait()->callback_ = CallOnQuit;
}

CallbackFunc* GetLeakySingleton() {
  return &CallbackSingleton::GetInstanceWithLeakTrait()->callback_;
}

void SingletonStatic(CallbackFunc CallOnQuit) {
  CallbackSingleton::GetInstanceWithStaticTrait()->callback_ = CallOnQuit;
}

CallbackFunc* GetStaticSingleton() {
  return &CallbackSingleton::GetInstanceWithStaticTrait()->callback_;
}

}  // namespace

class SingletonTest : public testing::Test {
 public:
  SingletonTest() { }

  virtual void SetUp() {
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

 protected:
  void VerifiesCallbacks() {
    EXPECT_TRUE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    EXPECT_TRUE(static_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

  void VerifiesCallbacksNotCalled() {
    EXPECT_FALSE(non_leak_called_);
    EXPECT_FALSE(leaky_called_);
    EXPECT_FALSE(static_called_);
    non_leak_called_ = false;
    leaky_called_ = false;
    static_called_ = false;
  }

  static void CallbackNoLeak() {
    non_leak_called_ = true;
  }

  static void CallbackLeak() {
    leaky_called_ = true;
  }

  static void CallbackStatic() {
    static_called_ = true;
  }

 private:
  static bool non_leak_called_;
  static bool leaky_called_;
  static bool static_called_;
};

bool SingletonTest::non_leak_called_ = false;
bool SingletonTest::leaky_called_ = false;
bool SingletonTest::static_called_ = false;

TEST_F(SingletonTest, Basic) {
  int* singleton_int_1;
  int* singleton_int_2;
  int* singleton_int_3;
  int* singleton_int_4;
  int* singleton_int_5;
  CallbackFunc* leaky_singleton;
  CallbackFunc* static_singleton;

  {
    base::ShadowingAtExitManager sem;
    {
      singleton_int_1 = SingletonInt1();
    }
    // Ensure POD type initialization.
    EXPECT_EQ(*singleton_int_1, 0);
    *singleton_int_1 = 1;

    EXPECT_EQ(singleton_int_1, SingletonInt1());
    EXPECT_EQ(*singleton_int_1, 1);

    {
      singleton_int_2 = SingletonInt2();
    }
    // Same instance that 1.
    EXPECT_EQ(*singleton_int_2, 1);
    EXPECT_EQ(singleton_int_1, singleton_int_2);

    {
      singleton_int_3 = SingletonInt3();
    }
    // Different instance than 1 and 2.
    EXPECT_EQ(*singleton_int_3, 0);
    EXPECT_NE(singleton_int_1, singleton_int_3);
    *singleton_int_3 = 3;
    EXPECT_EQ(*singleton_int_1, 1);
    EXPECT_EQ(*singleton_int_2, 1);

    {
      singleton_int_4 = SingletonInt4();
    }
    // Use a lock for creation. Not really tested at length.
    EXPECT_EQ(*singleton_int_4, 0);
    *singleton_int_4 = 4;
    EXPECT_NE(singleton_int_1, singleton_int_4);
    EXPECT_NE(singleton_int_3, singleton_int_4);

    {
      singleton_int_5 = SingletonInt5();
    }
    // Is default initialized to 5.
    EXPECT_EQ(*singleton_int_5, 5);
    EXPECT_NE(singleton_int_1, singleton_int_5);
    EXPECT_NE(singleton_int_3, singleton_int_5);
    EXPECT_NE(singleton_int_4, singleton_int_5);

    SingletonNoLeak(&CallbackNoLeak);
    SingletonLeak(&CallbackLeak);
    SingletonStatic(&CallbackStatic);
    static_singleton = GetStaticSingleton();
    leaky_singleton = GetLeakySingleton();
    EXPECT_TRUE(leaky_singleton);
  }

  // Verify that only the expected callback has been called.
  VerifiesCallbacks();
  // Delete the leaky singleton. It is interesting to note that Purify does
  // *not* detect the leak when this call is commented out. :(
  DefaultSingletonTraits<CallbackFunc>::Delete(leaky_singleton);

  // The static singleton can't be acquired post-atexit.
  EXPECT_EQ(NULL, GetStaticSingleton());

  {
    base::ShadowingAtExitManager sem;
    // Verifiy that the variables were reset.
    {
      singleton_int_1 = SingletonInt1();
      EXPECT_EQ(*singleton_int_1, 0);
    }
    {
      singleton_int_5 = SingletonInt5();
      EXPECT_EQ(*singleton_int_5, 5);
    }
    {
      // Resurrect the static singleton, and assert that it
      // still points to the same (static) memory.
      StaticMemorySingletonTraits<CallbackSingleton>::Resurrect();
      EXPECT_EQ(GetStaticSingleton(), static_singleton);
    }
  }
  // The leaky singleton shouldn't leak since SingletonLeak has not been called.
  VerifiesCallbacksNotCalled();
}
