// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_UTILS_H_
#define IPC_IPC_MESSAGE_UTILS_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/containers/stack_container.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/tuple.h"
#include "build/build_config.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_sync_message.h"

#if defined(COMPILER_GCC)
// GCC "helpfully" tries to inline template methods in release mode. Except we
// want the majority of the template junk being expanded once in the
// implementation file (and only provide the definitions in
// ipc_message_utils_impl.h in those files) and exported, instead of expanded
// at every call site. Special note: GCC happily accepts the attribute before
// the method declaration, but only acts on it if it is after.
#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40500
// Starting in gcc 4.5, the noinline no longer implies the concept covered by
// the introduced noclone attribute, which will create specialized versions of
// functions/methods when certain types are constant.
// www.gnu.org/software/gcc/gcc-4.5/changes.html
#define IPC_MSG_NOINLINE  __attribute__((noinline, noclone));
#else
#define IPC_MSG_NOINLINE  __attribute__((noinline));
#endif
#elif defined(COMPILER_MSVC)
// MSVC++ doesn't do this.
#define IPC_MSG_NOINLINE
#else
#error "Please add the noinline property for your new compiler here."
#endif

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class NullableString16;
class Time;
class TimeDelta;
class TimeTicks;
struct FileDescriptor;

#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)
class SharedMemoryHandle;
#endif  // (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)
}

namespace IPC {

struct ChannelHandle;

// -----------------------------------------------------------------------------
// How we send IPC message logs across channels.
struct IPC_EXPORT LogData {
  LogData();
  ~LogData();

  std::string channel;
  int32_t routing_id;
  uint32_t type;  // "User-defined" message type, from ipc_message.h.
  std::string flags;
  int64_t sent;  // Time that the message was sent (i.e. at Send()).
  int64_t receive;  // Time before it was dispatched (i.e. before calling
                    // OnMessageReceived).
  int64_t dispatch;  // Time after it was dispatched (i.e. after calling
                     // OnMessageReceived).
  std::string message_name;
  std::string params;
};

//-----------------------------------------------------------------------------

// A dummy struct to place first just to allow leading commas for all
// members in the macro-generated constructor initializer lists.
struct NoParams {
};

template <class P>
static inline void WriteParam(base::Pickle* m, const P& p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Write(m, static_cast<const Type& >(p));
}

template <class P>
static inline bool WARN_UNUSED_RESULT ReadParam(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                P* p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  return ParamTraits<Type>::Read(m, iter, reinterpret_cast<Type* >(p));
}

template <class P>
static inline void LogParam(const P& p, std::string* l) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Log(static_cast<const Type& >(p), l);
}

// Primitive ParamTraits -------------------------------------------------------

template <>
struct ParamTraits<bool> {
  typedef bool param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteBool(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadBool(r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<signed char> {
  typedef signed char param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<unsigned char> {
  typedef unsigned char param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<unsigned short> {
  typedef unsigned short param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<int> {
  typedef int param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteInt(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt(r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned int> {
  typedef unsigned int param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteInt(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt(reinterpret_cast<int*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<long> {
  typedef long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteLongUsingDangerousNonPortableLessPersistableForm(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadLong(r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long> {
  typedef unsigned long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteLongUsingDangerousNonPortableLessPersistableForm(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadLong(reinterpret_cast<long*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<long long> {
  typedef long long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteInt64(static_cast<int64_t>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt64(reinterpret_cast<int64_t*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long long> {
  typedef unsigned long long param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteInt64(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt64(reinterpret_cast<int64_t*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

// Note that the IPC layer doesn't sanitize NaNs and +/- INF values.  Clients
// should be sure to check the sanity of these values after receiving them over
// IPC.
template <>
struct IPC_EXPORT ParamTraits<float> {
  typedef float param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteFloat(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadFloat(r);
  }
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<double> {
  typedef double param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// STL ParamTraits -------------------------------------------------------------

template <>
struct ParamTraits<std::string> {
  typedef std::string param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteString(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadString(r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<base::string16> {
  typedef base::string16 param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteString16(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadString16(r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<std::vector<char> > {
  typedef std::vector<char> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle*,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<std::vector<unsigned char> > {
  typedef std::vector<unsigned char> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<std::vector<bool> > {
  typedef std::vector<bool> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <class P>
struct ParamTraits<std::vector<P> > {
  typedef std::vector<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    // ReadLength() checks for < 0 itself.
    if (!iter->ReadLength(&size))
      return false;
    // Resizing beforehand is not safe, see BUG 1006367 for details.
    if (INT_MAX / sizeof(P) <= static_cast<size_t>(size))
      return false;
    r->resize(size);
    for (int i = 0; i < size; i++) {
      if (!ReadParam(m, iter, &(*r)[i]))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam((p[i]), l);
    }
  }
};

template <class P>
struct ParamTraits<std::set<P> > {
  typedef std::set<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter)
      WriteParam(m, *iter);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!iter->ReadLength(&size))
      return false;
    for (int i = 0; i < size; ++i) {
      P item;
      if (!ReadParam(m, iter, &item))
        return false;
      r->insert(item);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::set>");
  }
};

template <class K, class V, class C, class A>
struct ParamTraits<std::map<K, V, C, A> > {
  typedef std::map<K, V, C, A> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::map>");
  }
};

template <class A, class B>
struct ParamTraits<std::pair<A, B> > {
  typedef std::pair<A, B> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.first);
    WriteParam(m, p.second);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &r->first) && ReadParam(m, iter, &r->second);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.first, l);
    l->append(", ");
    LogParam(p.second, l);
    l->append(")");
  }
};

// IPC ParamTraits -------------------------------------------------------------
template <>
struct IPC_EXPORT ParamTraits<BrokerableAttachment::AttachmentId> {
  typedef BrokerableAttachment::AttachmentId param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// Base ParamTraits ------------------------------------------------------------

template <>
struct IPC_EXPORT ParamTraits<base::DictionaryValue> {
  typedef base::DictionaryValue param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_POSIX)
// FileDescriptors may be serialised over IPC channels on POSIX. On the
// receiving side, the FileDescriptor is a valid duplicate of the file
// descriptor which was transmitted: *it is not just a copy of the integer like
// HANDLEs on Windows*. The only exception is if the file descriptor is < 0. In
// this case, the receiving end will see a value of -1. *Zero is a valid file
// descriptor*.
//
// The received file descriptor will have the |auto_close| flag set to true. The
// code which handles the message is responsible for taking ownership of it.
// File descriptors are OS resources and must be closed when no longer needed.
//
// When sending a file descriptor, the file descriptor must be valid at the time
// of transmission. Since transmission is not synchronous, one should consider
// dup()ing any file descriptors to be transmitted and setting the |auto_close|
// flag, which causes the file descriptor to be closed after writing.
template<>
struct IPC_EXPORT ParamTraits<base::FileDescriptor> {
  typedef base::FileDescriptor param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // defined(OS_POSIX)

#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)
template <>
struct IPC_EXPORT ParamTraits<base::SharedMemoryHandle> {
  typedef base::SharedMemoryHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)

template <>
struct IPC_EXPORT ParamTraits<base::FilePath> {
  typedef base::FilePath param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::ListValue> {
  typedef base::ListValue param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::NullableString16> {
  typedef base::NullableString16 param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::File::Info> {
  typedef base::File::Info param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<base::File::Error> {
  typedef int Type;
};

#if defined(OS_WIN)
template <>
struct SimilarTypeTraits<HWND> {
  typedef HANDLE Type;
};
#endif  // defined(OS_WIN)

template <>
struct IPC_EXPORT ParamTraits<base::Time> {
  typedef base::Time param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::TimeDelta> {
  typedef base::TimeDelta param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::TimeTicks> {
  typedef base::TimeTicks param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<base::Tuple<>> {
  typedef base::Tuple<> param_type;
  static void Write(base::Pickle* m, const param_type& p) {}
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

template <class A>
struct ParamTraits<base::Tuple<A>> {
  typedef base::Tuple<A> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::get<0>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &base::get<0>(*r));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(base::get<0>(p), l);
  }
};

template <class A, class B>
struct ParamTraits<base::Tuple<A, B>> {
  typedef base::Tuple<A, B> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::get<0>(p));
    WriteParam(m, base::get<1>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return (ReadParam(m, iter, &base::get<0>(*r)) &&
            ReadParam(m, iter, &base::get<1>(*r)));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(base::get<0>(p), l);
    l->append(", ");
    LogParam(base::get<1>(p), l);
  }
};

template <class A, class B, class C>
struct ParamTraits<base::Tuple<A, B, C>> {
  typedef base::Tuple<A, B, C> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::get<0>(p));
    WriteParam(m, base::get<1>(p));
    WriteParam(m, base::get<2>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return (ReadParam(m, iter, &base::get<0>(*r)) &&
            ReadParam(m, iter, &base::get<1>(*r)) &&
            ReadParam(m, iter, &base::get<2>(*r)));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(base::get<0>(p), l);
    l->append(", ");
    LogParam(base::get<1>(p), l);
    l->append(", ");
    LogParam(base::get<2>(p), l);
  }
};

template <class A, class B, class C, class D>
struct ParamTraits<base::Tuple<A, B, C, D>> {
  typedef base::Tuple<A, B, C, D> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::get<0>(p));
    WriteParam(m, base::get<1>(p));
    WriteParam(m, base::get<2>(p));
    WriteParam(m, base::get<3>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return (ReadParam(m, iter, &base::get<0>(*r)) &&
            ReadParam(m, iter, &base::get<1>(*r)) &&
            ReadParam(m, iter, &base::get<2>(*r)) &&
            ReadParam(m, iter, &base::get<3>(*r)));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(base::get<0>(p), l);
    l->append(", ");
    LogParam(base::get<1>(p), l);
    l->append(", ");
    LogParam(base::get<2>(p), l);
    l->append(", ");
    LogParam(base::get<3>(p), l);
  }
};

template <class A, class B, class C, class D, class E>
struct ParamTraits<base::Tuple<A, B, C, D, E>> {
  typedef base::Tuple<A, B, C, D, E> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::get<0>(p));
    WriteParam(m, base::get<1>(p));
    WriteParam(m, base::get<2>(p));
    WriteParam(m, base::get<3>(p));
    WriteParam(m, base::get<4>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return (ReadParam(m, iter, &base::get<0>(*r)) &&
            ReadParam(m, iter, &base::get<1>(*r)) &&
            ReadParam(m, iter, &base::get<2>(*r)) &&
            ReadParam(m, iter, &base::get<3>(*r)) &&
            ReadParam(m, iter, &base::get<4>(*r)));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(base::get<0>(p), l);
    l->append(", ");
    LogParam(base::get<1>(p), l);
    l->append(", ");
    LogParam(base::get<2>(p), l);
    l->append(", ");
    LogParam(base::get<3>(p), l);
    l->append(", ");
    LogParam(base::get<4>(p), l);
  }
};

template<class P>
struct ParamTraits<ScopedVector<P> > {
  typedef ScopedVector<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, *p[i]);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size = 0;
    if (!iter->ReadLength(&size))
      return false;
    if (INT_MAX/sizeof(P) <= static_cast<size_t>(size))
      return false;
    r->resize(size);
    for (int i = 0; i < size; i++) {
      (*r)[i] = new P();
      if (!ReadParam(m, iter, (*r)[i]))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam(*p[i], l);
    }
  }
};

template <class P, size_t stack_capacity>
struct ParamTraits<base::StackVector<P, stack_capacity> > {
  typedef base::StackVector<P, stack_capacity> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p->size()));
    for (size_t i = 0; i < p->size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    // ReadLength() checks for < 0 itself.
    if (!iter->ReadLength(&size))
      return false;
    // Sanity check for the vector size.
    if (INT_MAX / sizeof(P) <= static_cast<size_t>(size))
      return false;
    P value;
    for (int i = 0; i < size; i++) {
      if (!ReadParam(m, iter, &value))
        return false;
      (*r)->push_back(value);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p->size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam((p[i]), l);
    }
  }
};

template <typename NormalMap,
          int kArraySize,
          typename EqualKey,
          typename MapInit>
struct ParamTraits<base::SmallMap<NormalMap, kArraySize, EqualKey, MapInit> > {
  typedef base::SmallMap<NormalMap, kArraySize, EqualKey, MapInit> param_type;
  typedef typename param_type::key_type K;
  typedef typename param_type::data_type V;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!iter->ReadLength(&size))
      return false;
    for (int i = 0; i < size; ++i) {
      K key;
      if (!ReadParam(m, iter, &key))
        return false;
      V& value = (*r)[key];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<base::SmallMap>");
  }
};

template <class P>
struct ParamTraits<scoped_ptr<P> > {
  typedef scoped_ptr<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    bool valid = !!p;
    WriteParam(m, valid);
    if (valid)
      WriteParam(m, *p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    bool valid = false;
    if (!ReadParam(m, iter, &valid))
      return false;

    if (!valid) {
      r->reset();
      return true;
    }

    param_type temp(new P());
    if (!ReadParam(m, iter, temp.get()))
      return false;

    r->swap(temp);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    if (p)
      LogParam(*p, l);
    else
      l->append("NULL");
  }
};

// IPC types ParamTraits -------------------------------------------------------

// A ChannelHandle is basically a platform-inspecific wrapper around the
// fact that IPC endpoints are handled specially on POSIX.  See above comments
// on FileDescriptor for more background.
template<>
struct IPC_EXPORT ParamTraits<IPC::ChannelHandle> {
  typedef ChannelHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<LogData> {
  typedef LogData param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<Message> {
  static void Write(base::Pickle* m, const Message& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   Message* r);
  static void Log(const Message& p, std::string* l);
};

// Windows ParamTraits ---------------------------------------------------------

#if defined(OS_WIN)
template <>
struct IPC_EXPORT ParamTraits<HANDLE> {
  typedef HANDLE param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<LOGFONT> {
  typedef LOGFONT param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<MSG> {
  typedef MSG param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // defined(OS_WIN)

//-----------------------------------------------------------------------------
// Generic message subclasses

// Used for asynchronous messages.
template <class ParamType>
class MessageSchema {
 public:
  typedef ParamType Param;
  typedef typename base::TupleTypes<ParamType>::ParamTuple RefParam;

  static void Write(Message* msg, const RefParam& p) IPC_MSG_NOINLINE;
  static bool Read(const Message* msg, Param* p) IPC_MSG_NOINLINE;
};

// defined in ipc_logging.cc
IPC_EXPORT void GenerateLogData(const std::string& channel,
                                const Message& message,
                                LogData* data, bool get_params);


#if defined(IPC_MESSAGE_LOG_ENABLED)
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {
  const std::string& output_params = msg->output_params();
  if (!l->empty() && !output_params.empty())
    l->append(", ");

  l->append(output_params);
}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {
  if (msg->received_time() != 0) {
    std::string output_params;
    LogParam(reply_params, &output_params);
    msg->set_output_params(output_params);
  }
}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {
  if (msg->sent_time()) {
    // Don't log the sync message after dispatch, as we don't have the
    // output parameters at that point.  Instead, save its data and log it
    // with the outgoing reply message when it's sent.
    LogData* data = new LogData;
    GenerateLogData("", *msg, data, true);
    msg->set_dont_log();
    reply->set_sync_log_data(data);
  }
}
#else
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {}
#endif

// This class assumes that its template argument is a RefTuple (a Tuple with
// reference elements). This would go into ipc_message_utils_impl.h, but it is
// also used by chrome_frame.
template <class RefTuple>
class ParamDeserializer : public MessageReplyDeserializer {
 public:
  explicit ParamDeserializer(const RefTuple& out) : out_(out) { }

  bool SerializeOutputParameters(const IPC::Message& msg,
                                 base::PickleIterator iter) override {
    return ReadParam(&msg, &iter, &out_);
  }

  RefTuple out_;
};

// Used for synchronous messages.
template <class SendParamType, class ReplyParamType>
class SyncMessageSchema {
 public:
  typedef SendParamType SendParam;
  typedef typename base::TupleTypes<SendParam>::ParamTuple RefSendParam;
  typedef ReplyParamType ReplyParam;

  static void Write(Message* msg, const RefSendParam& send) IPC_MSG_NOINLINE;
  static bool ReadSendParam(const Message* msg, SendParam* p) IPC_MSG_NOINLINE;
  static bool ReadReplyParam(
      const Message* msg,
      typename base::TupleTypes<ReplyParam>::ValueTuple* p) IPC_MSG_NOINLINE;

  template<class T, class S, class Method>
  static bool DispatchWithSendParams(bool ok, const SendParam& send_params,
                                     const Message* msg, T* obj, S* sender,
                                     Method func) {
    Message* reply = SyncMessage::GenerateReply(msg);
    if (ok) {
      typename base::TupleTypes<ReplyParam>::ValueTuple reply_params;
      base::DispatchToMethod(obj, func, send_params, &reply_params);
      WriteParam(reply, reply_params);
      LogReplyParamsToMessage(reply_params, msg);
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
    }
    sender->Send(reply);
    return ok;
  }

  template<class T, class Method>
  static bool DispatchDelayReplyWithSendParams(bool ok,
                                               const SendParam& send_params,
                                               const Message* msg, T* obj,
                                               Method func) {
    Message* reply = SyncMessage::GenerateReply(msg);
    if (ok) {
      base::Tuple<Message&> t = base::MakeRefTuple(*reply);
      ConnectMessageAndReply(msg, reply);
      base::DispatchToMethod(obj, func, send_params, &t);
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      obj->Send(reply);
    }
    return ok;
  }

  template <typename... Ts>
  static void WriteReplyParams(Message* reply, Ts... args) {
    ReplyParam p(args...);
    WriteParam(reply, p);
  }
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_H_
