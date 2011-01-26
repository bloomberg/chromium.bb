// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
#define GPU_COMMAND_BUFFER_COMMON_LOGGING_H_

#include <assert.h>

#include <iostream>

// Windows defines an ERROR macro.
#ifdef ERROR
#undef ERROR
#endif

namespace gpu {

// Members are uppercase instead of kCamelCase for consistency with base log
// severity enum.
enum LogLevel {
  INFO,
  WARNING,
  ERROR,
  FATAL
};

// This is a very simple logger for use in command buffer code. Common and
// command buffer code cannot be dependent on base. It just outputs the message
// to stderr.
class Logger {
 public:
  Logger(bool condition, LogLevel level)
      : condition_(condition),
        level_(level) {
  }

  template <typename X>
  static Logger CheckTrue(const X& x,
                          const char* file, int line,
                          const char* x_name,
                          const char* check_name) {
    return Logger(!!x, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckEqual(const X& x, const Y& y,
                           const char* file, int line,
                           const char* x_name, const char* y_name,
                           const char* check_name) {
    return Logger(x == y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckNotEqual(const X& x, const Y& y,
                              const char* file, int line,
                              const char* x_name, const char* y_name,
                              const char* check_name) {
    return Logger(x != y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckLessThan(const X& x, const Y& y,
                              const char* file, int line,
                              const char* x_name, const char* y_name,
                              const char* check_name) {
    return Logger(x < y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckGreaterThan(const X& x, const Y& y,
                                 const char* file, int line,
                                 const char* x_name, const char* y_name,
                                 const char* check_name) {
    return Logger(x > y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckLessEqual(const X& x, const Y& y,
                               const char* file, int line,
                               const char* x_name, const char* y_name,
                               const char* check_name) {
    return Logger(x <= y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  template <typename X, typename Y>
  static Logger CheckGreaterEqual(const X& x, const Y& y,
                                  const char* file, int line,
                                  const char* x_name, const char* y_name,
                                  const char* check_name) {
    return Logger(x >= y, FATAL)
        << file << "(" << line << "): " << check_name
        << "(" << x_name << " (" << x << "), "
        << y_name << "(" << y << ")) failed. ";
  }

  ~Logger() {
    if (!condition_) {
      std::cerr << std::endl;
      std::cerr.flush();
      if (level_ == FATAL)
        assert(false);
    }
  }

  template <typename T>
  Logger& operator<<(const T& value) {
    if (!condition_)
      std::cerr << value;
    return *this;
  }

 private:
  Logger(const Logger& logger): condition_(logger.condition_) {
  }

  bool condition_;
  LogLevel level_;
};

}  // namespace gpu

#define GPU_CHECK(X) ::gpu::Logger::CheckTrue( \
    (X), __FILE__, __LINE__, #X, "GPU_CHECK")
#define GPU_CHECK_EQ(X, Y) ::gpu::Logger::CheckEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_EQ")
#define GPU_CHECK_NE(X, Y) ::gpu::Logger::CheckNotEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_NE")
#define GPU_CHECK_GT(X, Y) ::gpu::Logger::CheckGreaterThan( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_GT")
#define GPU_CHECK_LT(X, Y) ::gpu::Logger::CheckLessThan( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_LT")
#define GPU_CHECK_GE(X, Y) ::gpu::Logger::CheckGreaterEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_GE")
#define GPU_CHECK_LE(X, Y) ::gpu::Logger::CheckLessEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_CHECK_LE")
#define GPU_LOG(LEVEL) ::gpu::Logger(false, LEVEL)

#if defined(NDEBUG)

#define GPU_DCHECK(X) ::gpu::Logger::CheckTrue( \
    true, __FILE__, __LINE__, #X, "GPU_DCHECK")
#define GPU_DCHECK_EQ(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_EQ")
#define GPU_DCHECK_NE(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_NE")
#define GPU_DCHECK_GT(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_GT")
#define GPU_DCHECK_LT(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_LT")
#define GPU_DCHECK_GE(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_GE")
#define GPU_DCHECK_LE(X, Y) ::gpu::Logger::CheckEqual( \
    false, false, __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_LE")
#define GPU_DLOG(LEVEL) ::gpu::Logger(false, LEVEL)

#else  // NDEBUG

#define GPU_DCHECK(X) ::gpu::Logger::CheckTrue( \
    (X), __FILE__, __LINE__, #X, "GPU_DCHECK")
#define GPU_DCHECK_EQ(X, Y) ::gpu::Logger::CheckEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_EQ")
#define GPU_DCHECK_NE(X, Y) ::gpu::Logger::CheckNotEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_NE")
#define GPU_DCHECK_GT(X, Y) ::gpu::Logger::CheckGreaterThan( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_GT")
#define GPU_DCHECK_LT(X, Y) ::gpu::Logger::CheckLessThan( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_LT")
#define GPU_DCHECK_GE(X, Y) ::gpu::Logger::CheckGreaterEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_GE")
#define GPU_DCHECK_LE(X, Y) ::gpu::Logger::CheckLessEqual( \
    (X), (Y), __FILE__, __LINE__, #X, #Y, "GPU_DCHECK_LE")
#define GPU_DLOG(LEVEL) ::gpu::Logger(true, LEVEL)

#endif  // NDEBUG

#define GPU_NOTREACHED() GPU_DCHECK(false)

#endif  // GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
