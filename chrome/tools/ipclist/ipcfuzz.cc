// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <ostream>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/all_messages.h"
#include "content/common/all_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace IPC {
class Message;

// Interface implemented by those who fuzz basic types.  The types all
// correspond to the types which a pickle from base/pickle.h can pickle,
// plus the floating point types.
class Fuzzer {
 public:
  // Select a message for fuzzing.
  virtual bool FuzzThisMessage(const IPC::Message *msg) = 0;

  // Tweak individual values within a message.
  virtual void FuzzBool(bool* value) = 0;
  virtual void FuzzInt(int* value) = 0;
  virtual void FuzzLong(long* value) = 0;
  virtual void FuzzSize(size_t* value) = 0;
  virtual void FuzzUChar(unsigned char *value) = 0;
  virtual void FuzzUInt16(uint16* value) = 0;
  virtual void FuzzUInt32(uint32* value) = 0;
  virtual void FuzzInt64(int64* value) = 0;
  virtual void FuzzUInt64(uint64* value) = 0;
  virtual void FuzzFloat(float *value) = 0;
  virtual void FuzzDouble(double *value) = 0;
  virtual void FuzzString(std::string* value) = 0;
  virtual void FuzzWString(std::wstring* value) = 0;
  virtual void FuzzString16(string16* value) = 0;
  virtual void FuzzData(char* data, int length) = 0;
  virtual void FuzzBytes(void* data, int data_len) = 0;
};

}  // namespace IPC

namespace {

template <typename T>
void FuzzIntegralType(T* value, unsigned int frequency) {
  if (rand() % frequency == 0) {
    switch (rand() % 4) {
      case 0: (*value) = 0; break;
      case 1: (*value)--; break;
      case 2: (*value)++; break;
      case 3: (*value) ^= rand(); break;
    }
  }
}

template <typename T>
void FuzzStringType(T* value, unsigned int frequency,
                    const T& literal1, const T& literal2) {
  if (rand() % frequency == 0) {
    switch (rand() % 5) {
      case 4: (*value) = (*value) + (*value);   // FALLTHROUGH
      case 3: (*value) = (*value) + (*value);   // FALLTHROUGH
      case 2: (*value) = (*value) + (*value); break;
      case 1: (*value) += literal1; break;
      case 0: (*value) = literal2; break;
    }
  }
}

}  // namespace

// One such fuzzer implementation.
class DefaultFuzzer : public IPC::Fuzzer {
 public:
  static const int DEFAULT_FREQUENCY = 23;

  DefaultFuzzer() : frequency_(DEFAULT_FREQUENCY) {
    const char *env_var;
    if ((env_var = getenv("CHROME_IPC_FUZZING_LIST"))) {
      std::string str = std::string(env_var);
      size_t pos;
      while ((pos = str.find_first_of(',')) != std::string::npos) {
        message_set_.insert(atoi(str.substr(0, pos).c_str()));
        str = str.substr(pos+1);
      }
      message_set_.insert(atoi(str.c_str()));
    }

    if ((env_var = getenv("CHROME_IPC_FUZZING_SEED"))) {
      int new_seed = atoi(env_var);
      if (new_seed)
        srand(new_seed);
    }

    if ((env_var = getenv("CHROME_IPC_FUZZING_FREQUENCY"))) {
      unsigned int new_frequency = atoi(env_var);
      if (new_frequency)
        frequency_ = new_frequency;
    }
  }

  virtual ~DefaultFuzzer() {}

  virtual bool FuzzThisMessage(const IPC::Message *msg) OVERRIDE {
    return (message_set_.empty() ||
            std::find(message_set_.begin(),
                      message_set_.end(),
                      msg->type()) != message_set_.end());
  }

  virtual void FuzzBool(bool* value) OVERRIDE {
    if (rand() % frequency_ == 0)
      (*value) = !(*value);
  }

  virtual void FuzzInt(int* value) OVERRIDE {
    FuzzIntegralType<int>(value, frequency_);
  }

  virtual void FuzzLong(long* value) OVERRIDE {
    FuzzIntegralType<long>(value, frequency_);
  }

  virtual void FuzzSize(size_t* value) OVERRIDE {
    FuzzIntegralType<size_t>(value, frequency_);
  }

  virtual void FuzzUChar(unsigned char* value) OVERRIDE {
    FuzzIntegralType<unsigned char>(value, frequency_);
  }

  virtual void FuzzUInt16(uint16* value) OVERRIDE {
    FuzzIntegralType<uint16>(value, frequency_);
  }

  virtual void FuzzUInt32(uint32* value) OVERRIDE {
    FuzzIntegralType<uint32>(value, frequency_);
  }

  virtual void FuzzInt64(int64* value) OVERRIDE {
    FuzzIntegralType<int64>(value, frequency_);
  }

  virtual void FuzzUInt64(uint64* value) OVERRIDE {
    FuzzIntegralType<uint64>(value, frequency_);
  }

  virtual void FuzzFloat(float* value) OVERRIDE {
    if (rand() % frequency_ == 0)
      (*value) *= rand() / 1000000.0;
  }

  virtual void FuzzDouble(double* value) OVERRIDE {
    if (rand() % frequency_ == 0)
      (*value) *= rand() / 1000000.0;
  }

  virtual void FuzzString(std::string* value) OVERRIDE {
    FuzzStringType<std::string>(value, frequency_, "BORKED", "");
  }

  virtual void FuzzWString(std::wstring* value) OVERRIDE {
    FuzzStringType<std::wstring>(value, frequency_, L"BORKED", L"");
  }

  virtual void FuzzString16(string16* value) OVERRIDE {
    FuzzStringType<string16>(value, frequency_,
                             base::WideToUTF16(L"BORKED"),
                             base::WideToUTF16(L""));
  }

  virtual void FuzzData(char* data, int length) OVERRIDE {
    if (rand() % frequency_ == 0) {
      for (int i = 0; i < length; ++i) {
        FuzzIntegralType<char>(&data[i], frequency_);
      }
    }
  }

  virtual void FuzzBytes(void* data, int data_len) OVERRIDE {
    FuzzData(static_cast<char*>(data), data_len);
  }

 private:
  std::set<int> message_set_;
  unsigned int frequency_;
};


// No-op fuzzer.  Rewrites each message unchanged to check if the message
// re-assembly is legit.
class NoOpFuzzer : public IPC::Fuzzer {
 public:
  NoOpFuzzer() {}
  virtual ~NoOpFuzzer() {}

  virtual bool FuzzThisMessage(const IPC::Message *msg) OVERRIDE {
    return true;
  }

  virtual void FuzzBool(bool* value) OVERRIDE {}
  virtual void FuzzInt(int* value) OVERRIDE {}
  virtual void FuzzLong(long* value) OVERRIDE {}
  virtual void FuzzSize(size_t* value) OVERRIDE {}
  virtual void FuzzUChar(unsigned char* value) OVERRIDE {}
  virtual void FuzzUInt16(uint16* value) OVERRIDE {}
  virtual void FuzzUInt32(uint32* value) OVERRIDE {}
  virtual void FuzzInt64(int64* value) OVERRIDE {}
  virtual void FuzzUInt64(uint64* value) OVERRIDE {}
  virtual void FuzzFloat(float* value) OVERRIDE {}
  virtual void FuzzDouble(double* value) OVERRIDE {}
  virtual void FuzzString(std::string* value) OVERRIDE {}
  virtual void FuzzWString(std::wstring* value) OVERRIDE {}
  virtual void FuzzString16(string16* value) OVERRIDE {}
  virtual void FuzzData(char* data, int length) OVERRIDE {}
  virtual void FuzzBytes(void* data, int data_len) OVERRIDE {}
};

class FuzzerFactory {
 public:
  static IPC::Fuzzer *NewFuzzer(const std::string& name) {
    if (name == "no-op")
      return new NoOpFuzzer();
    else
      return new DefaultFuzzer();
  }
};

// Partially-specialized class that knows how to fuzz a given type.
template <class P>
struct FuzzTraits {
  static void Fuzz(P* p, IPC::Fuzzer *fuzzer) {}
};

// Template function to invoke partially-specialized class method.
template <class P>
static void FuzzParam(P* p, IPC::Fuzzer* fuzzer) {
  FuzzTraits<P>::Fuzz(p, fuzzer);
}

// Specializations to fuzz primitive types.
template <>
struct FuzzTraits<bool> {
  static void Fuzz(bool* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzBool(p);
  }
};

template <>
struct FuzzTraits<int> {
  static void Fuzz(int* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzInt(p);
  }
};

template <>
struct FuzzTraits<unsigned int> {
  static void Fuzz(unsigned int* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzInt(reinterpret_cast<int*>(p));
  }
};

template <>
struct FuzzTraits<long> {
  static void Fuzz(long* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzLong(p);
  }
};

template <>
struct FuzzTraits<unsigned long> {
  static void Fuzz(unsigned long* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzLong(reinterpret_cast<long*>(p));
  }
};

template <>
struct FuzzTraits<long long> {
  static void Fuzz(long long* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
  }
};

template <>
struct FuzzTraits<unsigned long long> {
  static void Fuzz(unsigned long long* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
  }
};

template <>
struct FuzzTraits<short> {
  static void Fuzz(short* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
  }
};

template <>
struct FuzzTraits<unsigned short> {
  static void Fuzz(unsigned short* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
  }
};

template <>
struct FuzzTraits<char> {
  static void Fuzz(char* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(reinterpret_cast<unsigned char*>(p));
  }
};

template <>
struct FuzzTraits<unsigned char> {
  static void Fuzz(unsigned char* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(p);
  }
};

template <>
struct FuzzTraits<float> {
  static void Fuzz(float* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzFloat(p);
  }
};

template <>
struct FuzzTraits<double> {
  static void Fuzz(double* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzDouble(p);
  }
};

template <>
struct FuzzTraits<std::string> {
  static void Fuzz(std::string* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzString(p);
  }
};

template <>
struct FuzzTraits<std::wstring> {
  static void Fuzz(std::wstring* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzWString(p);
  }
};

template <>
struct FuzzTraits<string16> {
  static void Fuzz(string16* p, IPC::Fuzzer* fuzzer) {
    fuzzer->FuzzString16(p);
  }
};

// Specializations to fuzz tuples.
template <class A>
struct FuzzTraits<Tuple1<A> > {
  static void Fuzz(Tuple1<A>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
  }
};

template <class A, class B>
struct FuzzTraits<Tuple2<A, B> > {
  static void Fuzz(Tuple2<A, B>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
  }
};

template <class A, class B, class C>
struct FuzzTraits<Tuple3<A, B, C> > {
  static void Fuzz(Tuple3<A, B, C>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<Tuple4<A, B, C, D> > {
  static void Fuzz(Tuple4<A, B, C, D>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
    FuzzParam(&p->d, fuzzer);
  }
};

template <class A, class B, class C, class D, class E>
struct FuzzTraits<Tuple5<A, B, C, D, E> > {
  static void Fuzz(Tuple5<A, B, C, D, E>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->a, fuzzer);
    FuzzParam(&p->b, fuzzer);
    FuzzParam(&p->c, fuzzer);
    FuzzParam(&p->d, fuzzer);
    FuzzParam(&p->e, fuzzer);
  }
};

// Specializations to fuzz containers.
template <class A>
struct FuzzTraits<std::vector<A> > {
  static void Fuzz(std::vector<A>* p, IPC::Fuzzer* fuzzer) {
    for (size_t i = 0; i < p->size(); ++i) {
      FuzzParam(&p->at(i), fuzzer);
    }
  }
};

template <class A, class B>
struct FuzzTraits<std::map<A, B> > {
  static void Fuzz(std::map<A, B>* p, IPC::Fuzzer* fuzzer) {
    typename std::map<A, B>::iterator it;
    for (it = p->begin(); it != p->end(); ++it) {
      FuzzParam(&it->second, fuzzer);
    }
  }
};

template <class A, class B>
struct FuzzTraits<std::pair<A, B> > {
  static void Fuzz(std::pair<A, B>* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->second, fuzzer);
  }
};

// Specializations to fuzz hand-coded tyoes
template <>
struct FuzzTraits<base::FileDescriptor> {
  static void Fuzz(base::FileDescriptor* p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->fd, fuzzer);
  }
};

template <>
struct FuzzTraits<GURL> {
  static void Fuzz(GURL *p, IPC::Fuzzer* fuzzer) {
    FuzzParam(&p->possibly_invalid_spec(), fuzzer);
  }
};

template <>
struct FuzzTraits<gfx::Point> {
  static void Fuzz(gfx::Point *p, IPC::Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    FuzzParam(&x, fuzzer);
    FuzzParam(&y, fuzzer);
    p->SetPoint(x, y);
  }
};

template <>
struct FuzzTraits<gfx::Size> {
  static void Fuzz(gfx::Size *p, IPC::Fuzzer* fuzzer) {
    int w = p->width();
    int h = p->height();
    FuzzParam(&w, fuzzer);
    FuzzParam(&h, fuzzer);
    p->SetSize(w, h);
  }
};

template <>
struct FuzzTraits<gfx::Rect> {
  static void Fuzz(gfx::Rect *p, IPC::Fuzzer* fuzzer) {
    gfx::Point origin = p->origin();
    gfx::Size  size = p->size();
    FuzzParam(&origin, fuzzer);
    FuzzParam(&size, fuzzer);
    p->set_origin(origin);
    p->set_size(size);
  }
};

// Means for updating message id in pickles.
class PickleCracker : public Pickle {
 public:
  static void CopyMessageID(PickleCracker *dst, PickleCracker *src) {
    memcpy(dst->mutable_payload(), src->payload(), sizeof(int));
  }
};

// Redefine macros to generate fuzzing from traits declarations.
// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent)\
  IPC_STRUCT_BEGIN(struct_name)
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate fuzz trait classes.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  template <> \
  struct FuzzTraits<struct_name> { \
    static void Fuzz(struct_name *p, IPC::Fuzzer* fuzzer) { \

#define IPC_STRUCT_TRAITS_MEMBER(name) \
      FuzzParam(&p->name, fuzzer);

#define IPC_STRUCT_TRAITS_PARENT(type) \
      FuzzParam(static_cast<type*>(p), fuzzer);

#define IPC_STRUCT_TRAITS_END() \
    } \
  };

#undef IPC_ENUM_TRAITS
#define IPC_ENUM_TRAITS(enum_name) \
  template <> \
  struct FuzzTraits<enum_name> { \
    static void Fuzz(enum_name* p, IPC::Fuzzer* fuzzer) { \
      FuzzParam(reinterpret_cast<int*>(p), fuzzer); \
    } \
  };

// Bring them into existence.
#include "chrome/common/all_messages.h"
#include "content/common/all_messages.h"

// Redefine macros to generate fuzzing funtions
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist)           \
  IPC_##kind##_##type##_FUZZ(name, in, out, ilist, olist)

#define IPC_EMPTY_CONTROL_FUZZ(name, in, out, ilist, olist)                 \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    return NULL;                                                            \
  }

#define IPC_EMPTY_ROUTED_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    return NULL;                                                            \
  }

#define IPC_ASYNC_CONTROL_FUZZ(name, in, out, ilist, olist)                 \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::Read(real_msg, &p);                                               \
    FuzzParam(&p, fuzzer);                                                  \
    return new name(IPC_MEMBERS_IN_##in(p));                                \
  }

#define IPC_ASYNC_ROUTED_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::Read(real_msg, &p);                                               \
    FuzzParam(&p, fuzzer);                                                  \
    return new name(msg->routing_id()                                       \
                    IPC_COMMA_##in                                          \
                    IPC_MEMBERS_IN_##in(p));                                \
  }

#define IPC_SYNC_CONTROL_FUZZ(name, in, out, ilist, olist)                  \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::ReadSendParam(real_msg, &p);                                      \
    FuzzParam(&p, fuzzer);                                                  \
    name* new_msg = new name(IPC_MEMBERS_IN_##in(p)                         \
                             IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                             IPC_MEMBERS_OUT_##out());                      \
    PickleCracker::CopyMessageID(                                           \
        reinterpret_cast<PickleCracker *>(new_msg),                         \
        reinterpret_cast<PickleCracker *>(real_msg));                       \
    return new_msg;                                                         \
  }


#define IPC_SYNC_ROUTED_FUZZ(name, in, out, ilist, olist)                   \
  IPC::Message* fuzzer_for_##name(IPC::Message *msg, IPC::Fuzzer* fuzzer) { \
    name* real_msg = static_cast<name*>(msg);                               \
    IPC_TUPLE_IN_##in ilist p;                                              \
    name::ReadSendParam(real_msg, &p);                                      \
    FuzzParam(&p, fuzzer);                                                  \
    name* new_msg = new name(msg->routing_id()                              \
                             IPC_COMMA_OR_##out(IPC_COMMA_##in)             \
                             IPC_MEMBERS_IN_##in(p)                         \
                             IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                             IPC_MEMBERS_OUT_##out());                      \
    PickleCracker::CopyMessageID(                                           \
        reinterpret_cast<PickleCracker *>(new_msg),                         \
        reinterpret_cast<PickleCracker *>(real_msg));                       \
    return new_msg;                                                         \
  }

#define IPC_MEMBERS_IN_0(p)
#define IPC_MEMBERS_IN_1(p)                 p.a
#define IPC_MEMBERS_IN_2(p)                 p.a, p.b
#define IPC_MEMBERS_IN_3(p)                 p.a, p.b, p.c
#define IPC_MEMBERS_IN_4(p)                 p.a, p.b, p.c, p.d
#define IPC_MEMBERS_IN_5(p)                 p.a, p.b, p.c, p.d, p.e

#define IPC_MEMBERS_OUT_0()
#define IPC_MEMBERS_OUT_1()                NULL
#define IPC_MEMBERS_OUT_2()                NULL, NULL
#define IPC_MEMBERS_OUT_3()                NULL, NULL, NULL
#define IPC_MEMBERS_OUT_4()                NULL, NULL, NULL, NULL
#define IPC_MEMBERS_OUT_5()                NULL, NULL, NULL, NULL, NULL

#include "chrome/common/all_messages.h"
#include "content/common/all_messages.h"

typedef IPC::Message* (*FuzzFunction)(IPC::Message*, IPC::Fuzzer*);
typedef base::hash_map<uint32, FuzzFunction> FuzzFunctionMap;

// Redefine macros to register fuzzing functions into map.
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  (*map)[static_cast<uint32>(name::ID)] = fuzzer_for_##name;

void PopulateFuzzFunctionMap(FuzzFunctionMap *map) {
#include "chrome/common/all_messages.h"
#include "content/common/all_messages.h"
}

class ipcfuzz : public IPC::ChannelProxy::OutgoingMessageFilter {
 public:
  ipcfuzz() {
    const char* env_var = getenv("CHROME_IPC_FUZZING_KIND");
    fuzzer_ = FuzzerFactory::NewFuzzer(env_var ? env_var : "");
    PopulateFuzzFunctionMap(&fuzz_function_map_);
  }

  virtual IPC::Message* Rewrite(IPC::Message* message) OVERRIDE {
    if (fuzzer_ && fuzzer_->FuzzThisMessage(message)) {
      FuzzFunctionMap::iterator it = fuzz_function_map_.find(message->type());
      if (it != fuzz_function_map_.end()) {
        IPC::Message* fuzzed_message = (*it->second)(message, fuzzer_);
        if (fuzzed_message)  {
          delete message;
          message = fuzzed_message;
        }
      }
    }
    return message;
  }

 private:
  IPC::Fuzzer* fuzzer_;
  FuzzFunctionMap fuzz_function_map_;
};

ipcfuzz g_ipcfuzz;

// Entry point avoiding mangled names.
extern "C" {
  __attribute__((visibility("default")))
  IPC::ChannelProxy::OutgoingMessageFilter* GetFilter(void);
}

IPC::ChannelProxy::OutgoingMessageFilter* GetFilter(void) {
  return &g_ipcfuzz;
}
