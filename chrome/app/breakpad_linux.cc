// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/file_version_info_linux.h"
#include "base/global_descriptors_posix.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "breakpad/src/client/linux/handler/exception_handler.h"
#include "breakpad/src/client/linux/minidump_writer/directory_reader.h"
#include "breakpad/src/common/linux/linux_libc_support.h"
#include "breakpad/src/common/linux/linux_syscall_support.h"
#include "breakpad/src/common/linux/memory.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "webkit/glue/plugins/plugin_list.h"

static const char kUploadURL[] =
    "https://clients2.google.com/cr/report";

static uint64_t uptime = 0;

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
    sys_close(dumpfd);
    return -1;
  }

  google_breakpad::PageAllocator allocator;

  uint8_t* dump_data = reinterpret_cast<uint8_t*>(allocator.Alloc(st.st_size));
  if (!dump_data) {
    static const char msg[] = "Cannot upload crash dump: cannot alloc\n";
    sys_write(2, msg, sizeof(msg));
    sys_close(dumpfd);
    return -1;
  }

  sys_read(dumpfd, dump_data, st.st_size);
  sys_close(dumpfd);

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
      read(ufd, &t, sizeof(t));
      write_uint64_hex(temp_file + sizeof(temp_file) - (16 + 1), t);

      fd = sys_open(temp_file, O_WRONLY | O_CREAT | O_EXCL, 0600);
      if (fd >= 0)
        break;
    }

    if (fd < 0) {
      static const char msg[] = "Failed to create temporary file in /tmp: "
          "cannot upload crash dump\n";
      sys_write(2, msg, sizeof(msg) - 1);
      sys_close(ufd);
      return -1;
    }
  } else {
    fd = sys_open(info.filename, O_WRONLY, 0600);
    if (fd < 0) {
      static const char msg[] = "Failed to save crash dump: failed to open\n";
      sys_write(2, msg, sizeof(msg) - 1);
      sys_close(ufd);
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
  sys_close(ufd);

  // The define for the product version is a wide string, so we need to
  // downconvert it.
  static const wchar_t version[] = PRODUCT_VERSION;
  static const unsigned version_len = sizeof(version) / sizeof(wchar_t);
  char version_msg[version_len];
  for (unsigned i = 0; i < version_len; ++i)
    version_msg[i] = static_cast<char>(version[i]);

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
  //   Content-Disposition: form-data; name="plugin-chunk-1" \r\n \r\n (0..5)
  //   abcdef \r\n (6, 7)
  //   BOUNDARY \r\n (8, 9)
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
  static const char plugin_chunk_msg[] = "plugin-chunk-";
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

  sys_writev(fd, iov, 29);

  if (uptime >= 0) {
    struct timeval tv;
    if (!gettimeofday(&tv, NULL)) {
      uint64_t time = timeval_to_ms(&tv);
      if (time > uptime) {
        time -= uptime;
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

        sys_writev(fd, iov, 9);
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

    sys_writev(fd, iov, 9);
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

    sys_writev(fd, iov, 9);
  }

  // For browser process.
  if (info.plugin_list_length) {
    unsigned i = 0, done = 0, plugin_length = info.plugin_list_length;
    static const unsigned kMaxPluginChunkSize = 64;
    static const unsigned kMaxPluginLength = 8 * kMaxPluginChunkSize;
    if (plugin_length > kMaxPluginLength)
      plugin_length = kMaxPluginLength;

    while (plugin_length) {
      char num[16];
      const unsigned num_len = my_int_len(++i);
      my_itos(num, i, num_len);

      iov[0].iov_base = const_cast<char*>(form_data_msg);
      iov[0].iov_len = sizeof(form_data_msg) - 1;
      iov[1].iov_base = const_cast<char*>(plugin_chunk_msg);
      iov[1].iov_len = sizeof(plugin_chunk_msg) - 1;
      iov[2].iov_base = num;
      iov[2].iov_len = num_len;
      iov[3].iov_base = const_cast<char*>(quote_msg);
      iov[3].iov_len = sizeof(quote_msg);
      iov[4].iov_base = const_cast<char*>(rn);
      iov[4].iov_len = sizeof(rn);
      iov[5].iov_base = const_cast<char*>(rn);
      iov[5].iov_len = sizeof(rn);

      const unsigned len = plugin_length > kMaxPluginChunkSize ?
                           kMaxPluginChunkSize : plugin_length;
      iov[6].iov_base = const_cast<char*>(info.plugin_list + done);
      iov[6].iov_len = len;
      iov[7].iov_base = const_cast<char*>(rn);
      iov[7].iov_len = sizeof(rn);
      iov[8].iov_base = mime_boundary;
      iov[8].iov_len = sizeof(mime_boundary) - 1;
      iov[9].iov_base = const_cast<char*>(rn);
      iov[9].iov_len = sizeof(rn);

      sys_writev(fd, iov, 10);

      done += len;
      plugin_length -= len;
    }
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

      sys_writev(fd, iov, 10);

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

  sys_writev(fd, iov, 11);

  sys_close(fd);

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
        sys_close(i);
    } else {
      google_breakpad::DirectoryReader reader(fd);
      const char* name;
      while (reader.GetNextEntry(&name)) {
        int i;
        if (my_strtoui(&i, name) && i > 2 && i != fd)
          sys_close(fd);
        reader.PopEntry();
      }

      sys_close(fd);
    }

    sys_setsid();

    // Leave one end of a pipe in the wget process and watch for it getting
    // closed by the wget process exiting.
    int fds[2];
    sys_pipe(fds);

    const pid_t child = sys_fork();
    if (child) {
      sys_close(fds[1]);
      char id_buf[17];
      const int len = HANDLE_EINTR(read(fds[0], id_buf, sizeof(id_buf) - 1));
      if (len > 0) {
        id_buf[len] = 0;
        static const char msg[] = "\nCrash dump id: ";
        sys_write(2, msg, sizeof(msg) - 1);
        sys_write(2, id_buf, my_strlen(id_buf));
        sys_write(2, "\n", 1);
      }
      sys_unlink(info.filename);
      sys_unlink(temp_file);
      sys__exit(0);
    }

    sys_close(fds[0]);
    sys_dup2(fds[1], 3);
    static const char* const kWgetBinary = "/usr/bin/wget";
    const char* args[] = {
      kWgetBinary,
      header,
      post_file,
      kUploadURL,
      "-O",  // output reply to fd 3
      "/dev/fd/3",
      NULL,
    };

    execv(kWgetBinary, const_cast<char**>(args));
    static const char msg[] = "Cannot upload crash dump: cannot exec "
                              "/usr/bin/wget\n";
    sys_write(2, msg, sizeof(msg) - 1);
    sys__exit(1);
  }

  return child;
}

// This is defined in chrome/browser/google_update_settings_posix.cc, it's the
// static string containing the user's unique GUID. We send this in the crash
// report.
namespace google_update {
extern std::string posix_guid;
}

// This is defined in base/linux_util.cc, it's the static string containing the
// user's distro info. We send this in the crash report.
namespace base {
extern std::string linux_distro;
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

  std::string* plugin_list = NPAPI::PluginList::GetLoadedPlugins();
  BreakpadInfo info;
  info.filename = path;
  info.process_type = "browser";
  info.process_type_length = 7;
  info.crash_url = NULL;
  info.crash_url_length = 0;
  info.guid = google_update::posix_guid.data();
  info.guid_length = google_update::posix_guid.length();
  info.distro = base::linux_distro.data();
  info.distro_length = base::linux_distro.length();
  info.plugin_list = plugin_list->data();
  info.plugin_list_length = plugin_list->length();
  info.upload = upload;
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
  if (unattended) {
    FilePath dumps_path("/tmp");
    PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
    new google_breakpad::ExceptionHandler(dumps_path.value().c_str(), NULL,
                                          CrashDoneNoUpload, NULL,
                                          true /* install handlers */);
  } else {
    new google_breakpad::ExceptionHandler("/tmp", NULL, CrashDoneUpload, NULL,
                                          true /* install handlers */);
  }
}

// This is defined in chrome/common/child_process_logging_linux.cc, it's the
// static string containing the current active URL. We send this in the crash
// report.
namespace child_process_logging {
extern std::string active_url;
}

// Currently Non-Browser = Renderer and Plugins
static bool
NonBrowserCrashHandler(const void* crash_context, size_t crash_context_size,
                       void* context) {
  const int fd = reinterpret_cast<intptr_t>(context);
  int fds[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  char guid[kGuidSize + 1] = {0};
  char crash_url[kMaxActiveURLSize + 1] = {0};
  char distro[kDistroSize + 1] = {0};
  const size_t guid_len = std::min(google_update::posix_guid.size(),
                                   kGuidSize);
  const size_t crash_url_len =
      std::min(child_process_logging::active_url.size(), kMaxActiveURLSize);
  const size_t distro_len =
      std::min(base::linux_distro.size(), kDistroSize);
  memcpy(guid, google_update::posix_guid.data(), guid_len);
  memcpy(crash_url, child_process_logging::active_url.data(), crash_url_len);
  memcpy(distro, base::linux_distro.data(), distro_len);

  // The length of the control message:
  static const unsigned kControlMsgSize = CMSG_SPACE(sizeof(int));

  struct kernel_msghdr msg;
  my_memset(&msg, 0, sizeof(struct kernel_msghdr));
  struct kernel_iovec iov[4];
  iov[0].iov_base = const_cast<void*>(crash_context);
  iov[0].iov_len = crash_context_size;
  iov[1].iov_base = guid;
  iov[1].iov_len = kGuidSize + 1;
  iov[2].iov_base = crash_url;
  iov[2].iov_len = kMaxActiveURLSize + 1;
  iov[3].iov_base = distro;
  iov[3].iov_len = kDistroSize + 1;

  msg.msg_iov = iov;
  msg.msg_iovlen = 4;
  char cmsg[kControlMsgSize];
  memset(cmsg, 0, kControlMsgSize);
  msg.msg_control = cmsg;
  msg.msg_controllen = sizeof(cmsg);

  struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_RIGHTS;
  hdr->cmsg_len = CMSG_LEN(sizeof(int));
  *((int*) CMSG_DATA(hdr)) = fds[1];

  HANDLE_EINTR(sys_sendmsg(fd, &msg, 0));
  sys_close(fds[1]);

  char b;
  HANDLE_EINTR(sys_read(fds[0], &b, 1));

  return true;
}

void EnableNonBrowserCrashDumping() {
  const int fd = Singleton<base::GlobalDescriptors>()->Get(kCrashDumpSignal);
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
  const bool unattended = (getenv("CHROME_HEADLESS") != NULL);
  if (process_type.empty()) {
    if (!(unattended || GoogleUpdateSettings::GetCollectStatsConsent()))
      return;
    EnableCrashDumping(unattended);
  } else if (process_type == switches::kRendererProcess ||
             process_type == switches::kPluginProcess ||
             process_type == switches::kZygoteProcess) {
    // We might be chrooted in a zygote or renderer process so we cannot call
    // GetCollectStatsConsent because that needs access the the user's home
    // dir. Instead, we set a command line flag for these processes.
    // Even though plugins are not chrooted, we share the same code path for
    // simplicity.
    if (!parsed_command_line.HasSwitch(switches::kEnableCrashReporter))
      return;
    // Get the guid and linux distro from the command line switch.
    std::string switch_value = WideToASCII(
        parsed_command_line.GetSwitchValue(switches::kEnableCrashReporter));
    size_t separator = switch_value.find(",");
    if (separator != std::string::npos) {
      google_update::posix_guid = switch_value.substr(0, separator);
      base::linux_distro = switch_value.substr(separator + 1);
    } else {
      google_update::posix_guid = switch_value;
    }
    EnableNonBrowserCrashDumping();
  }

  // Set the base process uptime value.
  struct timeval tv;
  if (!gettimeofday(&tv, NULL))
    uptime = timeval_to_ms(&tv);
  else
    uptime = 0;
}
