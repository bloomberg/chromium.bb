// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For linux_syscall_support.h. This makes it safe to call embedded system
// calls when in seccomp mode.
#define SYS_SYSCALL_ENTRYPOINT "playground$syscallEntryPoint"

#include "chrome/app/breakpad_linux.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/global_descriptors_posix.h"
#include "base/linux_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/minidump_writer/directory_reader.h"
#include "breakpad/src/common/linux/linux_libc_support.h"
#include "breakpad/src/common/memory.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info_posix.h"
#include "chrome/common/env_vars.h"
#include "seccompsandbox/linux_syscall_support.h"

// Some versions of gcc are prone to warn about unused return values. In cases
// where we either a) know the call cannot fail, or b) there is nothing we
// can do when a call fails, we mark the return code as ignored. This avoids
// spurious compiler warnings.
#define IGNORE_RET(x) do { if (x); } while (0)

static const char kUploadURL[] =
    "https://clients2.google.com/cr/report";

static bool is_crash_reporter_enabled = false;
static uint64_t process_start_time = 0;
static char* crash_log_path = NULL;

// Writes the value |v| as 16 hex characters to the memory pointed at by
// |output|.
static void write_uint64_hex(char* output, uint64_t v) {
  static const char hextable[] = "0123456789abcdef";

  for (int i = 15; i >= 0; --i) {
    output[i] = hextable[v & 15];
    v >>= 4;
  }
}

// The following helper functions are for calculating uptime.

// Converts a struct timeval to milliseconds.
static uint64_t timeval_to_ms(struct timeval *tv) {
  uint64_t ret = tv->tv_sec;  // Avoid overflow by explicitly using a uint64_t.
  ret *= 1000;
  ret += tv->tv_usec / 1000;
  return ret;
}

// Converts a struct timeval to milliseconds.
static uint64_t kernel_timeval_to_ms(struct kernel_timeval *tv) {
  uint64_t ret = tv->tv_sec;  // Avoid overflow by explicitly using a uint64_t.
  ret *= 1000;
  ret += tv->tv_usec / 1000;
  return ret;
}

// uint64_t version of my_int_len() from
// breakpad/src/common/linux/linux_libc_support.h. Return the length of the
// given, non-negative integer when expressed in base 10.
static unsigned my_uint64_len(uint64_t i) {
  if (!i)
    return 1;

  unsigned len = 0;
  while (i) {
    len++;
    i /= 10;
  }

  return len;
}

// uint64_t version of my_itos() from
// breakpad/src/common/linux/linux_libc_support.h. Convert a non-negative
// integer to a string.
static void my_uint64tos(char* output, uint64_t i, unsigned i_len) {
  for (unsigned index = i_len; index; --index, i /= 10)
    output[index - 1] = '0' + (i % 10);
}

pid_t HandleCrashDump(const BreakpadInfo& info) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.

  const int dumpfd = sys_open(info.filename, O_RDONLY, 0);
  if (dumpfd < 0) {
    static const char msg[] = "Cannot upload crash dump: failed to open\n";
    sys_write(2, msg, sizeof(msg));
    return -1;
  }
  struct kernel_stat st;
  if (sys_fstat(dumpfd, &st) != 0) {
    static const char msg[] = "Cannot upload crash dump: stat failed\n";
    sys_write(2, msg, sizeof(msg));
    IGNORE_RET(sys_close(dumpfd));
    return -1;
  }

  google_breakpad::PageAllocator allocator;

  uint8_t* dump_data = reinterpret_cast<uint8_t*>(allocator.Alloc(st.st_size));
  if (!dump_data) {
    static const char msg[] = "Cannot upload crash dump: cannot alloc\n";
    sys_write(2, msg, sizeof(msg));
    IGNORE_RET(sys_close(dumpfd));
    return -1;
  }

  sys_read(dumpfd, dump_data, st.st_size);
  IGNORE_RET(sys_close(dumpfd));

  // We need to build a MIME block for uploading to the server. Since we are
  // going to fork and run wget, it needs to be written to a temp file.

  const int ufd = sys_open("/dev/urandom", O_RDONLY, 0);
  if (ufd < 0) {
    static const char msg[] = "Cannot upload crash dump because /dev/urandom"
                              " is missing\n";
    sys_write(2, msg, sizeof(msg) - 1);
    return -1;
  }

  static const char temp_file_template[] =
      "/tmp/chromium-upload-XXXXXXXXXXXXXXXX";
  char temp_file[sizeof(temp_file_template)];
  int fd = -1;
  if (info.upload) {
    memcpy(temp_file, temp_file_template, sizeof(temp_file_template));

    for (unsigned i = 0; i < 10; ++i) {
      uint64_t t;
      sys_read(ufd, &t, sizeof(t));
      write_uint64_hex(temp_file + sizeof(temp_file) - (16 + 1), t);

      fd = sys_open(temp_file, O_WRONLY | O_CREAT | O_EXCL, 0600);
      if (fd >= 0)
        break;
    }

    if (fd < 0) {
      static const char msg[] = "Failed to create temporary file in /tmp: "
          "cannot upload crash dump\n";
      sys_write(2, msg, sizeof(msg) - 1);
      IGNORE_RET(sys_close(ufd));
      return -1;
    }
  } else {
    fd = sys_open(info.filename, O_WRONLY, 0600);
    if (fd < 0) {
      static const char msg[] = "Failed to save crash dump: failed to open\n";
      sys_write(2, msg, sizeof(msg) - 1);
      IGNORE_RET(sys_close(ufd));
      return -1;
    }
  }

  // The MIME boundary is 28 hyphens, followed by a 64-bit nonce and a NUL.
  char mime_boundary[28 + 16 + 1];
  my_memset(mime_boundary, '-', 28);
  uint64_t boundary_rand;
  sys_read(ufd, &boundary_rand, sizeof(boundary_rand));
  write_uint64_hex(mime_boundary + 28, boundary_rand);
  mime_boundary[28 + 16] = 0;
  IGNORE_RET(sys_close(ufd));

  static const char version_msg[] = PRODUCT_VERSION;

  // The MIME block looks like this:
  //   BOUNDARY \r\n (0, 1)
  //   Content-Disposition: form-data; name="prod" \r\n \r\n (2..6)
  //   Chrome_Linux \r\n (7, 8)
  //   BOUNDARY \r\n (9, 10)
  //   Content-Disposition: form-data; name="ver" \r\n \r\n (11..15)
  //   1.2.3.4 \r\n (16, 17)
  //   BOUNDARY \r\n (18, 19)
  //   Content-Disposition: form-data; name="guid" \r\n \r\n (20..24)
  //   1.2.3.4 \r\n (25, 26)
  //   BOUNDARY \r\n (27, 28)
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="ptime" \r\n \r\n (0..4)
  //   abcdef \r\n (5, 6)
  //   BOUNDARY \r\n (7, 8)
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="ptype" \r\n \r\n (0..4)
  //   abcdef \r\n (5, 6)
  //   BOUNDARY \r\n (7, 8)
  //
  //   zero or one:
  //   Content-Disposition: form-data; name="lsb-release" \r\n \r\n (0..4)
  //   abcdef \r\n (5, 6)
  //   BOUNDARY \r\n (7, 8)
  //
  //   zero or more:
  //   Content-Disposition: form-data; name="url-chunk-1" \r\n \r\n (0..5)
  //   abcdef \r\n (6, 7)
  //   BOUNDARY \r\n (8, 9)
  //
  //   Content-Disposition: form-data; name="dump"; filename="dump" \r\n (0,1,2)
  //   Content-Type: application/octet-stream \r\n \r\n (3,4,5)
  //   <dump contents> (6)
  //   \r\n BOUNDARY -- \r\n (7,8,9,10)

  static const char rn[] = {'\r', '\n'};
  static const char form_data_msg[] = "Content-Disposition: form-data; name=\"";
  static const char prod_msg[] = "prod";
  static const char quote_msg[] = {'"'};
#if defined(OS_CHROMEOS)
  static const char chrome_product_msg[] = "Chrome_ChromeOS";
#else  // OS_LINUX
  static const char chrome_product_msg[] = "Chrome_Linux";
#endif
  static const char ver_msg[] = "ver";
  static const char guid_msg[] = "guid";
  static const char dashdash_msg[] = {'-', '-'};
  static const char dump_msg[] = "upload_file_minidump\"; filename=\"dump\"";
  static const char content_type_msg[] =
      "Content-Type: application/octet-stream";
  static const char url_chunk_msg[] = "url-chunk-";
  static const char process_time_msg[] = "ptime";
  static const char process_type_msg[] = "ptype";
  static const char distro_msg[] = "lsb-release";

  struct kernel_iovec iov[29];
  iov[0].iov_base = mime_boundary;
  iov[0].iov_len = sizeof(mime_boundary) - 1;
  iov[1].iov_base = const_cast<char*>(rn);
  iov[1].iov_len = sizeof(rn);

  iov[2].iov_base = const_cast<char*>(form_data_msg);
  iov[2].iov_len = sizeof(form_data_msg) - 1;
  iov[3].iov_base = const_cast<char*>(prod_msg);
  iov[3].iov_len = sizeof(prod_msg) - 1;
  iov[4].iov_base = const_cast<char*>(quote_msg);
  iov[4].iov_len = sizeof(quote_msg);
  iov[5].iov_base = const_cast<char*>(rn);
  iov[5].iov_len = sizeof(rn);
  iov[6].iov_base = const_cast<char*>(rn);
  iov[6].iov_len = sizeof(rn);

  iov[7].iov_base = const_cast<char*>(chrome_product_msg);
  iov[7].iov_len = sizeof(chrome_product_msg) - 1;
  iov[8].iov_base = const_cast<char*>(rn);
  iov[8].iov_len = sizeof(rn);

  iov[9].iov_base = mime_boundary;
  iov[9].iov_len = sizeof(mime_boundary) - 1;
  iov[10].iov_base = const_cast<char*>(rn);
  iov[10].iov_len = sizeof(rn);

  iov[11].iov_base = const_cast<char*>(form_data_msg);
  iov[11].iov_len = sizeof(form_data_msg) - 1;
  iov[12].iov_base = const_cast<char*>(ver_msg);
  iov[12].iov_len = sizeof(ver_msg) - 1;
  iov[13].iov_base = const_cast<char*>(quote_msg);
  iov[13].iov_len = sizeof(quote_msg);
  iov[14].iov_base = const_cast<char*>(rn);
  iov[14].iov_len = sizeof(rn);
  iov[15].iov_base = const_cast<char*>(rn);
  iov[15].iov_len = sizeof(rn);

  iov[16].iov_base = const_cast<char*>(version_msg);
  iov[16].iov_len = sizeof(version_msg) - 1;
  iov[17].iov_base = const_cast<char*>(rn);
  iov[17].iov_len = sizeof(rn);

  iov[18].iov_base = mime_boundary;
  iov[18].iov_len = sizeof(mime_boundary) - 1;
  iov[19].iov_base = const_cast<char*>(rn);
  iov[19].iov_len = sizeof(rn);

  iov[20].iov_base = const_cast<char*>(form_data_msg);
  iov[20].iov_len = sizeof(form_data_msg) - 1;
  iov[21].iov_base = const_cast<char*>(guid_msg);
  iov[21].iov_len = sizeof(guid_msg) - 1;
  iov[22].iov_base = const_cast<char*>(quote_msg);
  iov[22].iov_len = sizeof(quote_msg);
  iov[23].iov_base = const_cast<char*>(rn);
  iov[23].iov_len = sizeof(rn);
  iov[24].iov_base = const_cast<char*>(rn);
  iov[24].iov_len = sizeof(rn);

  iov[25].iov_base = const_cast<char*>(info.guid);
  iov[25].iov_len = info.guid_length;
  iov[26].iov_base = const_cast<char*>(rn);
  iov[26].iov_len = sizeof(rn);

  iov[27].iov_base = mime_boundary;
  iov[27].iov_len = sizeof(mime_boundary) - 1;
  iov[28].iov_base = const_cast<char*>(rn);
  iov[28].iov_len = sizeof(rn);

  IGNORE_RET(sys_writev(fd, iov, 29));

  if (info.process_start_time > 0) {
    struct kernel_timeval tv;
    if (!sys_gettimeofday(&tv, NULL)) {
      uint64_t time = kernel_timeval_to_ms(&tv);
      if (time > info.process_start_time) {
        time -= info.process_start_time;
        char time_str[21];
        const unsigned time_len = my_uint64_len(time);
        my_uint64tos(time_str, time, time_len);

        iov[0].iov_base = const_cast<char*>(form_data_msg);
        iov[0].iov_len = sizeof(form_data_msg) - 1;
        iov[1].iov_base = const_cast<char*>(process_time_msg);
        iov[1].iov_len = sizeof(process_time_msg) - 1;
        iov[2].iov_base = const_cast<char*>(quote_msg);
        iov[2].iov_len = sizeof(quote_msg);
        iov[3].iov_base = const_cast<char*>(rn);
        iov[3].iov_len = sizeof(rn);
        iov[4].iov_base = const_cast<char*>(rn);
        iov[4].iov_len = sizeof(rn);

        iov[5].iov_base = const_cast<char*>(time_str);
        iov[5].iov_len = time_len;
        iov[6].iov_base = const_cast<char*>(rn);
        iov[6].iov_len = sizeof(rn);
        iov[7].iov_base = mime_boundary;
        iov[7].iov_len = sizeof(mime_boundary) - 1;
        iov[8].iov_base = const_cast<char*>(rn);
        iov[8].iov_len = sizeof(rn);

        IGNORE_RET(sys_writev(fd, iov, 9));
      }
    }
  }

  if (info.process_type_length) {
    iov[0].iov_base = const_cast<char*>(form_data_msg);
    iov[0].iov_len = sizeof(form_data_msg) - 1;
    iov[1].iov_base = const_cast<char*>(process_type_msg);
    iov[1].iov_len = sizeof(process_type_msg) - 1;
    iov[2].iov_base = const_cast<char*>(quote_msg);
    iov[2].iov_len = sizeof(quote_msg);
    iov[3].iov_base = const_cast<char*>(rn);
    iov[3].iov_len = sizeof(rn);
    iov[4].iov_base = const_cast<char*>(rn);
    iov[4].iov_len = sizeof(rn);

    iov[5].iov_base = const_cast<char*>(info.process_type);
    iov[5].iov_len = info.process_type_length;
    iov[6].iov_base = const_cast<char*>(rn);
    iov[6].iov_len = sizeof(rn);
    iov[7].iov_base = mime_boundary;
    iov[7].iov_len = sizeof(mime_boundary) - 1;
    iov[8].iov_base = const_cast<char*>(rn);
    iov[8].iov_len = sizeof(rn);

    IGNORE_RET(sys_writev(fd, iov, 9));
  }

  if (info.distro_length) {
    iov[0].iov_base = const_cast<char*>(form_data_msg);
    iov[0].iov_len = sizeof(form_data_msg) - 1;
    iov[1].iov_base = const_cast<char*>(distro_msg);
    iov[1].iov_len = sizeof(distro_msg) - 1;
    iov[2].iov_base = const_cast<char*>(quote_msg);
    iov[2].iov_len = sizeof(quote_msg);
    iov[3].iov_base = const_cast<char*>(rn);
    iov[3].iov_len = sizeof(rn);
    iov[4].iov_base = const_cast<char*>(rn);
    iov[4].iov_len = sizeof(rn);

    iov[5].iov_base = const_cast<char*>(info.distro);
    iov[5].iov_len = info.distro_length;
    iov[6].iov_base = const_cast<char*>(rn);
    iov[6].iov_len = sizeof(rn);
    iov[7].iov_base = mime_boundary;
    iov[7].iov_len = sizeof(mime_boundary) - 1;
    iov[8].iov_base = const_cast<char*>(rn);
    iov[8].iov_len = sizeof(rn);

    IGNORE_RET(sys_writev(fd, iov, 9));
  }

  // For rendererers and plugins.
  if (info.crash_url_length) {
    unsigned i = 0, done = 0, crash_url_length = info.crash_url_length;
    static const unsigned kMaxCrashChunkSize = 64;
    static const unsigned kMaxUrlLength = 8 * kMaxCrashChunkSize;
    if (crash_url_length > kMaxUrlLength)
      crash_url_length = kMaxUrlLength;

    while (crash_url_length) {
      char num[16];
      const unsigned num_len = my_int_len(++i);
      my_itos(num, i, num_len);

      iov[0].iov_base = const_cast<char*>(form_data_msg);
      iov[0].iov_len = sizeof(form_data_msg) - 1;
      iov[1].iov_base = const_cast<char*>(url_chunk_msg);
      iov[1].iov_len = sizeof(url_chunk_msg) - 1;
      iov[2].iov_base = num;
      iov[2].iov_len = num_len;
      iov[3].iov_base = const_cast<char*>(quote_msg);
      iov[3].iov_len = sizeof(quote_msg);
      iov[4].iov_base = const_cast<char*>(rn);
      iov[4].iov_len = sizeof(rn);
      iov[5].iov_base = const_cast<char*>(rn);
      iov[5].iov_len = sizeof(rn);

      const unsigned len = crash_url_length > kMaxCrashChunkSize ?
                           kMaxCrashChunkSize : crash_url_length;
      iov[6].iov_base = const_cast<char*>(info.crash_url + done);
      iov[6].iov_len = len;
      iov[7].iov_base = const_cast<char*>(rn);
      iov[7].iov_len = sizeof(rn);
      iov[8].iov_base = mime_boundary;
      iov[8].iov_len = sizeof(mime_boundary) - 1;
      iov[9].iov_base = const_cast<char*>(rn);
      iov[9].iov_len = sizeof(rn);

      IGNORE_RET(sys_writev(fd, iov, 10));

      done += len;
      crash_url_length -= len;
    }
  }

  iov[0].iov_base = const_cast<char*>(form_data_msg);
  iov[0].iov_len = sizeof(form_data_msg) - 1;
  iov[1].iov_base = const_cast<char*>(dump_msg);
  iov[1].iov_len = sizeof(dump_msg) - 1;
  iov[2].iov_base = const_cast<char*>(rn);
  iov[2].iov_len = sizeof(rn);

  iov[3].iov_base = const_cast<char*>(content_type_msg);
  iov[3].iov_len = sizeof(content_type_msg) - 1;
  iov[4].iov_base = const_cast<char*>(rn);
  iov[4].iov_len = sizeof(rn);
  iov[5].iov_base = const_cast<char*>(rn);
  iov[5].iov_len = sizeof(rn);

  iov[6].iov_base = dump_data;
  iov[6].iov_len = st.st_size;

  iov[7].iov_base = const_cast<char*>(rn);
  iov[7].iov_len = sizeof(rn);
  iov[8].iov_base = mime_boundary;
  iov[8].iov_len = sizeof(mime_boundary) - 1;
  iov[9].iov_base = const_cast<char*>(dashdash_msg);
  iov[9].iov_len = sizeof(dashdash_msg);
  iov[10].iov_base = const_cast<char*>(rn);
  iov[10].iov_len = sizeof(rn);

  IGNORE_RET(sys_writev(fd, iov, 11));

  IGNORE_RET(sys_close(fd));

  if (!info.upload)
    return 0;

  // The --header argument to wget looks like:
  //   --header=Content-Type: multipart/form-data; boundary=XYZ
  // where the boundary has two fewer leading '-' chars
  static const char header_msg[] =
      "--header=Content-Type: multipart/form-data; boundary=";
  char* const header = reinterpret_cast<char*>(allocator.Alloc(
      sizeof(header_msg) - 1 + sizeof(mime_boundary) - 2));
  memcpy(header, header_msg, sizeof(header_msg) - 1);
  memcpy(header + sizeof(header_msg) - 1, mime_boundary + 2,
         sizeof(mime_boundary) - 2);
  // We grab the NUL byte from the end of |mime_boundary|.

  // The --post-file argument to wget looks like:
  //   --post-file=/tmp/...
  static const char post_file_msg[] = "--post-file=";
  char* const post_file = reinterpret_cast<char*>(allocator.Alloc(
       sizeof(post_file_msg) - 1 + sizeof(temp_file)));
  memcpy(post_file, post_file_msg, sizeof(post_file_msg) - 1);
  memcpy(post_file + sizeof(post_file_msg) - 1, temp_file, sizeof(temp_file));

  const pid_t child = sys_fork();
  if (!child) {
    // This code is called both when a browser is crashing (in which case,
    // nothing really matters any more) and when a renderer/plugin crashes, in
    // which case we need to continue.
    //
    // Since we are a multithreaded app, if we were just to fork(), we might
    // grab file descriptors which have just been created in another thread and
    // hold them open for too long.
    //
    // Thus, we have to loop and try and close everything.
    const int fd = sys_open("/proc/self/fd", O_DIRECTORY | O_RDONLY, 0);
    if (fd < 0) {
      for (unsigned i = 3; i < 8192; ++i)
        IGNORE_RET(sys_close(i));
    } else {
      google_breakpad::DirectoryReader reader(fd);
      const char* name;
      while (reader.GetNextEntry(&name)) {
        int i;
        if (my_strtoui(&i, name) && i > 2 && i != fd)
          IGNORE_RET(sys_close(i));
        reader.PopEntry();
      }

      IGNORE_RET(sys_close(fd));
    }

    IGNORE_RET(sys_setsid());

    // Leave one end of a pipe in the wget process and watch for it getting
    // closed by the wget process exiting.
    int fds[2];
    IGNORE_RET(sys_pipe(fds));

    const pid_t child = sys_fork();
    if (child) {
      IGNORE_RET(sys_close(fds[1]));
      char id_buf[17];
      const int len = HANDLE_EINTR(sys_read(fds[0], id_buf,
                                   sizeof(id_buf) - 1));
      if (len > 0) {
        // Write crash dump id to stderr.
        id_buf[len] = 0;
        static const char msg[] = "\nCrash dump id: ";
        sys_write(2, msg, sizeof(msg) - 1);
        sys_write(2, id_buf, my_strlen(id_buf));
        sys_write(2, "\n", 1);

        // Write crash dump id to crash log as: seconds_since_epoch,crash_id
        struct kernel_timeval tv;
        if (crash_log_path && !sys_gettimeofday(&tv, NULL)) {
          uint64_t time = kernel_timeval_to_ms(&tv) / 1000;
          char time_str[21];
          const unsigned time_len = my_uint64_len(time);
          my_uint64tos(time_str, time, time_len);

          int log_fd = sys_open(crash_log_path, O_CREAT | O_WRONLY | O_APPEND,
                                0600);
          if (log_fd > 0) {
            sys_write(log_fd, time_str, time_len);
            sys_write(log_fd, ",", 1);
            sys_write(log_fd, id_buf, my_strlen(id_buf));
            sys_write(log_fd, "\n", 1);
            IGNORE_RET(sys_close(log_fd));
          }
        }
      }
      IGNORE_RET(sys_unlink(info.filename));
      IGNORE_RET(sys_unlink(temp_file));
      sys__exit(0);
    }

    IGNORE_RET(sys_close(fds[0]));
    IGNORE_RET(sys_dup2(fds[1], 3));
    static const char* const kWgetBinary = "/usr/bin/wget";
    const char* args[] = {
      kWgetBinary,
      header,
      post_file,
      kUploadURL,
      "--timeout=10",  // Set a timeout so we don't hang forever.
      "--tries=1",     // Don't retry if the upload fails.
      "-O",  // output reply to fd 3
      "/dev/fd/3",
      NULL,
    };

    execve(kWgetBinary, const_cast<char**>(args), environ);
    static const char msg[] = "Cannot upload crash dump: cannot exec "
                              "/usr/bin/wget\n";
    sys_write(2, msg, sizeof(msg) - 1);
    sys__exit(1);
  }

  HANDLE_EINTR(sys_waitpid(child, NULL, 0));
  return child;
}

static bool CrashDone(const char* dump_path,
                      const char* minidump_id,
                      const bool upload,
                      const bool succeeded) {
  // WARNING: this code runs in a compromised context. It may not call into
  // libc nor allocate memory normally.
  if (!succeeded)
    return false;

  google_breakpad::PageAllocator allocator;
  const unsigned dump_path_len = my_strlen(dump_path);
  const unsigned minidump_id_len = my_strlen(minidump_id);
  char *const path = reinterpret_cast<char*>(allocator.Alloc(
      dump_path_len + 1 /* '/' */ + minidump_id_len +
      4 /* ".dmp" */ + 1 /* NUL */));
  memcpy(path, dump_path, dump_path_len);
  path[dump_path_len] = '/';
  memcpy(path + dump_path_len + 1, minidump_id, minidump_id_len);
  memcpy(path + dump_path_len + 1 + minidump_id_len, ".dmp", 4);
  path[dump_path_len + 1 + minidump_id_len + 4] = 0;

  BreakpadInfo info;
  info.filename = path;
  info.process_type = "browser";
  info.process_type_length = 7;
  info.crash_url = NULL;
  info.crash_url_length = 0;
  info.guid = child_process_logging::g_client_id;
  info.guid_length = my_strlen(child_process_logging::g_client_id);
  info.distro = base::g_linux_distro;
  info.distro_length = my_strlen(base::g_linux_distro);
  info.upload = upload;
  info.process_start_time = process_start_time;
  HandleCrashDump(info);

  return true;
}

// Wrapper script, do not add more code here.
static bool CrashDoneNoUpload(const char* dump_path,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  return CrashDone(dump_path, minidump_id, false, succeeded);
}

// Wrapper script, do not add more code here.
static bool CrashDoneUpload(const char* dump_path,
                      const char* minidump_id,
                      void* context,
                      bool succeeded) {
  return CrashDone(dump_path, minidump_id, true, succeeded);
}

void EnableCrashDumping(const bool unattended) {
  is_crash_reporter_enabled = true;

  FilePath tmp_path("/tmp");
  PathService::Get(base::DIR_TEMP, &tmp_path);

  FilePath dumps_path(tmp_path);
  if (PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path)) {
    FilePath logfile = dumps_path.AppendASCII("uploads.log");
    std::string logfile_str = logfile.value();
    const size_t crash_log_path_len = logfile_str.size() + 1;
    crash_log_path = new char[crash_log_path_len];
    strncpy(crash_log_path, logfile_str.c_str(), crash_log_path_len);
  }

  if (unattended) {
    new google_breakpad::ExceptionHandler(dumps_path.value().c_str(), NULL,
                                          CrashDoneNoUpload, NULL,
                                          true /* install handlers */);
  } else {
    new google_breakpad::ExceptionHandler(tmp_path.value().c_str(), NULL,
                                          CrashDoneUpload, NULL,
                                          true /* install handlers */);
  }
}

// Currently Non-Browser = Renderer, Plugins, Native Client and Gpu
static bool
NonBrowserCrashHandler(const void* crash_context, size_t crash_context_size,
                       void* context) {
  const int fd = reinterpret_cast<intptr_t>(context);
  int fds[2] = { -1, -1 };
  if (sys_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    static const char msg[] = "Failed to create socket for crash dumping.\n";
    sys_write(2, msg, sizeof(msg)-1);
    return false;
  }
  char guid[kGuidSize + 1] = {0};
  char crash_url[kMaxActiveURLSize + 1] = {0};
  char distro[kDistroSize + 1] = {0};
  const size_t guid_len =
      std::min(my_strlen(child_process_logging::g_client_id), kGuidSize);
  const size_t crash_url_len =
      std::min(my_strlen(child_process_logging::g_active_url),
               kMaxActiveURLSize);
  const size_t distro_len =
      std::min(my_strlen(base::g_linux_distro), kDistroSize);
  memcpy(guid, child_process_logging::g_client_id, guid_len);
  memcpy(crash_url, child_process_logging::g_active_url, crash_url_len);
  memcpy(distro, base::g_linux_distro, distro_len);

  char b;  // Dummy variable for sys_read below.
  const char* b_addr = &b;  // Get the address of |b| so we can create the
                            // expected /proc/[pid]/syscall content in the
                            // browser to convert namespace tids.

  // The length of the control message:
  static const unsigned kControlMsgSize = CMSG_SPACE(2*sizeof(int));

  const size_t kIovSize = 7;
  struct kernel_msghdr msg;
  my_memset(&msg, 0, sizeof(struct kernel_msghdr));
  struct kernel_iovec iov[kIovSize];
  iov[0].iov_base = const_cast<void*>(crash_context);
  iov[0].iov_len = crash_context_size;
  iov[1].iov_base = guid;
  iov[1].iov_len = kGuidSize + 1;
  iov[2].iov_base = crash_url;
  iov[2].iov_len = kMaxActiveURLSize + 1;
  iov[3].iov_base = distro;
  iov[3].iov_len = kDistroSize + 1;
  iov[4].iov_base = &b_addr;
  iov[4].iov_len = sizeof(b_addr);
  iov[5].iov_base = &fds[0];
  iov[5].iov_len = sizeof(fds[0]);
  iov[6].iov_base = &process_start_time;
  iov[6].iov_len = sizeof(process_start_time);

  msg.msg_iov = iov;
  msg.msg_iovlen = kIovSize;
  char cmsg[kControlMsgSize];
  my_memset(cmsg, 0, kControlMsgSize);
  msg.msg_control = cmsg;
  msg.msg_controllen = sizeof(cmsg);

  struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_RIGHTS;
  hdr->cmsg_len = CMSG_LEN(2*sizeof(int));
  ((int*) CMSG_DATA(hdr))[0] = fds[0];
  ((int*) CMSG_DATA(hdr))[1] = fds[1];

  if (HANDLE_EINTR(sys_sendmsg(fd, &msg, 0)) < 0) {
    static const char msg[] = "Failed to tell parent about crash.\n";
    sys_write(2, msg, sizeof(msg)-1);
    IGNORE_RET(sys_close(fds[1]));
    return false;
  }
  IGNORE_RET(sys_close(fds[1]));

  if (HANDLE_EINTR(sys_read(fds[0], &b, 1)) != 1) {
    static const char msg[] = "Parent failed to complete crash dump.\n";
    sys_write(2, msg, sizeof(msg)-1);
  }

  return true;
}

void EnableNonBrowserCrashDumping() {
  const int fd = base::GlobalDescriptors::GetInstance()->Get(kCrashDumpSignal);
  is_crash_reporter_enabled = true;
  // We deliberately leak this object.
  google_breakpad::ExceptionHandler* handler =
      new google_breakpad::ExceptionHandler("" /* unused */, NULL, NULL,
                                            (void*) fd, true);
  handler->set_crash_handler(NonBrowserCrashHandler);
}

void InitCrashReporter() {
  // Determine the process type and take appropriate action.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::string process_type =
      parsed_command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    EnableCrashDumping(getenv(env_vars::kHeadless) != NULL);
  } else if (process_type == switches::kRendererProcess ||
             process_type == switches::kPluginProcess ||
             process_type == switches::kZygoteProcess ||
             process_type == switches::kNaClLoaderProcess ||
             process_type == switches::kGpuProcess) {
    // We might be chrooted in a zygote or renderer process so we cannot call
    // GetCollectStatsConsent because that needs access the the user's home
    // dir. Instead, we set a command line flag for these processes.
    // Even though plugins are not chrooted, we share the same code path for
    // simplicity.
    if (!parsed_command_line.HasSwitch(switches::kEnableCrashReporter))
      return;
    // Get the guid and linux distro from the command line switch.
    std::string switch_value =
        parsed_command_line.GetSwitchValueASCII(switches::kEnableCrashReporter);
    size_t separator = switch_value.find(",");
    if (separator != std::string::npos) {
      child_process_logging::SetClientId(switch_value.substr(0, separator));
      base::SetLinuxDistro(switch_value.substr(separator + 1));
    } else {
      child_process_logging::SetClientId(switch_value);
    }
    EnableNonBrowserCrashDumping();
  }

  // Set the base process start time value.
  struct timeval tv;
  if (!gettimeofday(&tv, NULL))
    process_start_time = timeval_to_ms(&tv);
  else
    process_start_time = 0;
}

bool IsCrashReporterEnabled() {
  return is_crash_reporter_enabled;
}
