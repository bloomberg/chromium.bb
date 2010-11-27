// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for testing.

#ifndef CEEE_TESTING_UTILS_TEST_UTILS_H_
#define CEEE_TESTING_UTILS_TEST_UTILS_H_

#include <atlbase.h>
#include <atlcom.h>
#include <vector>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace testing {

// Counts the number of connections of type connection_point_id on container.
HRESULT GetConnectionCount(IUnknown* container,
                           REFIID connection_point_id,
                           size_t* num_connections);

// Fires event with dispid |id| on IConnectionPointImpl-derived connection
// point instance |cp| with the |num_args| arguments specified in |args|.
template <typename ConnectionPoint>
void FireEvent(ConnectionPoint *cp, DISPID id, size_t num_args, VARIANT* args) {
  typedef std::vector<IUnknown*> SinkVector;
  SinkVector sinks;

  // Grab all the sinks.
  sinks.insert(sinks.end(), cp->m_vec.begin(), cp->m_vec.end());

  // Grab a reference to them all in a loop.
  // This does not cause an IPC, so we don't need to worry about
  // reentrancy on the collection so far.
  SinkVector::const_iterator it(sinks.begin()), end(sinks.end());
  for (; it != end; ++it) {
    if (*it)
      (*it)->AddRef();
  }

  DISPPARAMS dispparams = { args, NULL, num_args, 0 };

  // And fire the event at them.
  for (it = sinks.begin(); it != sinks.end(); ++it) {
    CComDispatchDriver sink_disp(*it);

    if (sink_disp != NULL)
      sink_disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                        &dispparams, NULL, NULL, NULL);
  }

  // Release the reference we grabbed above.
  for (it = sinks.begin(); it != sinks.end(); ++it) {
    if (*it)
      (*it)->Release();
  }
}

// Prevents any logging actions (DCHECKs, killing the process,
// informational output, or anything) for the scope of its lifetime.
class LogDisabler {
 public:
  LogDisabler();
  ~LogDisabler();

 private:
  logging::LogMessageHandlerFunction old_handler_;
  static bool DropMessageHandler(int severity, const char* file, int line,
      size_t message_start, const std::string& str);
};

// Overrides a path in the PathService singleton, replacing the
// original at the end of its lifetime.
class PathServiceOverrider {
public:
  PathServiceOverrider(int key, const FilePath& path);
  ~PathServiceOverrider();

private:
  int key_;
  FilePath original_path_;
};

// GMock action to simplify writing the side effect of copying a string
// to an output parameter.
template <size_t N>
inline SetArrayArgumentActionP2<N, const char*, const char*>
    CopyStringToArgument(const char* s) {
  return SetArrayArgument<N>(s, s + lstrlenA(s) + 1);
}

template <size_t N>
inline SetArrayArgumentActionP2<N, const wchar_t*, const wchar_t*>
    CopyStringToArgument(const wchar_t* s) {
  return SetArrayArgument<N>(s, s + lstrlenW(s) + 1);
}

// GMock action to simplify writing the side effect of copying a wchar_t string
// to a BSTR output parameter. The wchar_t string must be in scope when the
// expected call is made.
ACTION_TEMPLATE(CopyBSTRToArgument,
                HAS_1_TEMPLATE_PARAMS(size_t, N),
                AND_1_VALUE_PARAMS(value)) {
  StaticAssertTypeEq<const wchar_t*, value_type>();
  *::std::tr1::get<N>(args) = ::SysAllocString(value);
}

namespace internal {

template <size_t N, typename InputInterface>
class CopyInterfaceToArgumentAction {
 public:
  // Constructs an action that sets the variable pointed to by the
  // N-th function argument to 'value'.
  explicit CopyInterfaceToArgumentAction(InputInterface* value)
      : value_(value) {}

  template <typename Result, typename ArgumentTuple>
  void Perform(const ArgumentTuple& args) const {
    CompileAssertTypesEqual<void, Result>();
    HRESULT hr = const_cast<CComPtr<InputInterface>&>(value_)
        .CopyTo(::std::tr1::get<N>(args));
    ASSERT_HRESULT_SUCCEEDED(hr);
  }

 private:
  CComPtr<InputInterface> value_;
};

template <size_t N>
class CopyVariantToArgumentAction {
 public:
  // Constructs an action that sets the variable pointed to by the
  // N-th function argument to 'value'.
  explicit CopyVariantToArgumentAction(const VARIANT& value) : value_(value) {}

  template <typename Result, typename ArgumentTuple>
  void Perform(const ArgumentTuple& args) const {
    CompileAssertTypesEqual<void, Result>();
    HRESULT hr = ::VariantCopy(::std::tr1::get<N>(args), &value_);
    ASSERT_HRESULT_SUCCEEDED(hr);
  }

 private:
  CComVariant value_;
};

void WriteVariant(const VARIANT& var, ::std::ostream* os);

// Comparator for variants.
class VariantMatcher : public ::testing::MatcherInterface<VARIANT> {
 public:
  explicit VariantMatcher(const VARIANT& value)
    : value_(value) {}

  virtual bool MatchAndExplain(VARIANT var,
                               MatchResultListener* listener) const {
    // listener is guaranteed not to be NULL by Google Mock, but its
    // stream may be NULL though (it sometimes uses DummyMatchResultListener).
    if (listener->stream() != NULL)
      DescribeTo(listener->stream());
    return value_ == var;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals ";
    WriteVariant(value_, os);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal ";
    WriteVariant(value_, os);
  }
 private:
  CComVariant value_;
};

// Comparator for variant pointers.
class VariantPointerMatcher : public ::testing::MatcherInterface<VARIANT*> {
 public:
  explicit VariantPointerMatcher(const VARIANT* value)
    : value_(value) {}

  virtual bool MatchAndExplain(VARIANT* var,
                               MatchResultListener* listener) const {
    // listener is guaranteed not to be NULL by Google Mock, but its
    // stream may be NULL though (it sometimes uses DummyMatchResultListener).
    if (listener->stream() != NULL)
      DescribeTo(listener->stream());
    return value_ == *var;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals ";
    WriteVariant(value_, os);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal ";
    WriteVariant(value_, os);
  }
 private:
  CComVariant value_;
};

template <size_t N>
class DispParamArgMatcher : public ::testing::MatcherInterface<DISPPARAMS*> {
 public:
  explicit DispParamArgMatcher(const VARIANT& value)
    : value_(value) {}

  virtual bool MatchAndExplain(DISPPARAMS* params,
                               MatchResultListener* listener) const {
    // listener is guaranteed not to be NULL by Google Mock, but its
    // stream may be NULL though (it sometimes uses DummyMatchResultListener).
    if (listener->stream() != NULL)
      DescribeTo(listener->stream());
    return params->cArgs >= N &&  value_ == params->rgvarg[N];
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals ";
    WriteVariant(value_, os);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal ";
    WriteVariant(value_, os);
  }
 private:
  CComVariant value_;
};

}  // namespace internal

// GMock action to simplify writing the side effect of copying an interface
// to an output parameter, observing COM reference counting rules.
template <size_t N, typename InputInterface>
PolymorphicAction<internal::CopyInterfaceToArgumentAction<N, InputInterface> >
CopyInterfaceToArgument(InputInterface* itf) {
  return MakePolymorphicAction(
      internal::CopyInterfaceToArgumentAction<N, InputInterface>(itf));
}
template <size_t N, typename InputInterface>
PolymorphicAction<internal::CopyInterfaceToArgumentAction<N, InputInterface> >
CopyInterfaceToArgument(const CComPtr<InputInterface> &itf) {
  return MakePolymorphicAction(
      internal::CopyInterfaceToArgumentAction<N, InputInterface>(itf));
}

// GMock action to simplify writing the side effect of copying a variant
// to an output parameter, using variant copy methods.
template <size_t N>
PolymorphicAction<internal::CopyVariantToArgumentAction<N> >
CopyVariantToArgument(const VARIANT& variant) {
  return MakePolymorphicAction(
      internal::CopyVariantToArgumentAction<N>(variant));
}

// GMock matcher to compare variants.
inline Matcher<VARIANT> VariantEq(const VARIANT& var) {
  return MakeMatcher(new internal::VariantMatcher(var));
}

// GMock matcher to compare variant pointers.
inline Matcher<VARIANT *> VariantPointerEq(const VARIANT* var) {
  return MakeMatcher(new internal::VariantPointerMatcher(var));
}

// GMock matcher to match argument N in a DISPPARAM pointer.
// This makes it humane to match on parameters to IDispatch::Invoke.
template <size_t N>
inline Matcher<DISPPARAMS*> DispParamArgEq(const VARIANT& var) {
  return MakeMatcher(new internal::DispParamArgMatcher<N>(var));
}

// Gmock action to add a reference to a COM object you are having a mock
// method return.
ACTION_P(AddRef, p) {
  p->AddRef();
}

// Temporarily overrides registry keys.
class ScopedRegistryOverride {
 public:
  ScopedRegistryOverride();
  ~ScopedRegistryOverride();

 private:
  void Override();

  base::win::RegKey hkcu_;
  base::win::RegKey hklm_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRegistryOverride);
};

}  // namespace testing

// Allow GMock to print VARIANT values.
inline ::std::ostream& operator<<(::std::ostream& os, const VARIANT& var) {
  testing::internal::WriteVariant(var, &os);
  return os;
}

// Sometimes our tests that use SideStep to stub out dependencies fail in
// release builds because one or more of the functions being hooked by
// SideStep is inlined by the compiler, and therefore cannot be hooked.
// In rare cases, we might use a noinline pragma to make such functions always
// hookable, but usually it is easier and better (so as not to impact the
// production code) to simply run the test in debug builds only.
//
// For noinline pragma, see
// http://msdn.microsoft.com/en-us/library/cx053bca(VS.71).aspx
#ifdef _DEBUG
#define TEST_DEBUG_ONLY(test_case_name, test_name) \
    TEST(test_case_name, test_name)
#define TEST_F_DEBUG_ONLY(test_fixture, test_name) \
    TEST_F(test_fixture, test_name)
#else
#define TEST_DEBUG_ONLY(test_case_name, test_name) \
    void DisabledTest##test_case_name##test_name()
#define TEST_F_DEBUG_ONLY TEST_DEBUG_ONLY
#endif



#endif  // CEEE_TESTING_UTILS_TEST_UTILS_H_
