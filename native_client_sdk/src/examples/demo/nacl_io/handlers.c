/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "handlers.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hello_nacl_io.h"

#define MAX_OPEN_FILES 10

/**
 * A mapping from int -> FILE*, so the JavaScript messages can refer to an open
 * File. */
static FILE* g_OpenFiles[MAX_OPEN_FILES];

/**
 * Add the file to the g_OpenFiles map.
 * @param[in] file The file to add to g_OpenFiles.
 * @return int The index of the FILE in g_OpenFiles, or -1 if there are too many
 *     open files. */
static int AddFileToMap(FILE* file) {
  int i;
  assert(file != NULL);
  for (i = 0; i < MAX_OPEN_FILES; ++i) {
    if (g_OpenFiles[i] == NULL) {
      g_OpenFiles[i] = file;
      return i;
    }
  }

  return -1;
}

/**
 * Remove the file from the g_OpenFiles map.
 * @param[in] i The index of the file handle to remove. */
static void RemoveFileFromMap(int i) {
  assert(i >= 0 && i < MAX_OPEN_FILES);
  g_OpenFiles[i] = NULL;
}

/**
 * Get a file handle from the g_OpenFiles map.
 * @param[in] i The index of the file handle to get.
 * @return the FILE*, or NULL of there is no open file with that handle.
 */
static FILE* GetFileFromMap(int i) {
  assert(i >= 0 && i < MAX_OPEN_FILES);
  return g_OpenFiles[i];
}

/**
 * Get a file, given a string containing the index.
 * @param[in] s The string containing the file index.
 * @param[out] file_index The file index of this file.
 * @return The FILE* for this file, or NULL if the index is invalid. */
static FILE* GetFileFromIndexString(const char* s, int* file_index) {
  char* endptr;
  int result = strtol(s, &endptr, 10);
  if (endptr != s + strlen(s)) {
    /* Garbage at the end of the number...? */
    return NULL;
  }

  if (file_index)
    *file_index = result;

  return GetFileFromMap(result);
}

/**
 * Handle a call to fopen() made by JavaScript.
 *
 * fopen expects 2 parameters:
 *   0: the path of the file to open
 *   1: the mode string
 * on success, fopen returns a result in |output| separated by \1:
 *   0: "fopen"
 *   1: the filename opened
 *   2: the file index
 * on failure, fopen returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleFopen(int num_params, char** params, char** output) {
  FILE* file;
  int file_index;
  const char* filename;
  const char* mode;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fopen takes 2 parameters.");
    return 1;
  }

  filename = params[0];
  mode = params[1];

  file = fopen(filename, mode);
  if (!file) {
    *output = PrintfToNewString("Error: fopen returned a NULL FILE*.");
    return 2;
  }

  file_index = AddFileToMap(file);
  if (file_index == -1) {
    *output = PrintfToNewString(
        "Error: Example only allows %d open file handles.", MAX_OPEN_FILES);
    return 3;
  }

  *output = PrintfToNewString("fopen\1%s\1%d", filename, file_index);
  return 0;
}

/**
 * Handle a call to fwrite() made by JavaScript.
 *
 * fwrite expects 2 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: A string to write to the file
 * on success, fwrite returns a result in |output| separated by \1:
 *   0: "fwrite"
 *   1: the file index
 *   2: the number of bytes written
 * on failure, fwrite returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleFwrite(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  const char* data;
  size_t data_len;
  size_t bytes_written;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fwrite takes 2 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  data = params[1];
  data_len = strlen(data);

  if (!file) {
    *output =
        PrintfToNewString("Error: Unknown file handle %s.", file_index_string);
    return 2;
  }

  bytes_written = fwrite(data, 1, data_len, file);

  if (ferror(file)) {
    *output = PrintfToNewString(
        "Error: Wrote %d bytes, but ferror() returns true.", bytes_written);
    return 3;
  }

  *output =
      PrintfToNewString("fwrite\1%s\1%d", file_index_string, bytes_written);
  return 0;
}

/**
 * Handle a call to fread() made by JavaScript.
 *
 * fread expects 2 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: The number of bytes to read from the file.
 * on success, fread returns a result in |output| separated by \1:
 *   0: "fread"
 *   1: the file index
 *   2: the data read from the file
 * on failure, fread returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleFread(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  char* buffer;
  size_t data_len;
  size_t bytes_read;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fread takes 2 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  data_len = strtol(params[1], NULL, 10);

  if (!file) {
    *output =
        PrintfToNewString("Error: Unknown file handle %s.", file_index_string);
    return 2;
  }

  buffer = (char*)malloc(data_len + 1);
  bytes_read = fread(buffer, 1, data_len, file);
  buffer[bytes_read] = 0;

  if (ferror(file)) {
    *output = PrintfToNewString(
        "Error: Read %d bytes, but ferror() returns true.", bytes_read);
    return 3;
  }

  *output = PrintfToNewString("fread\1%s\1%s", file_index_string, buffer);
  free(buffer);
  return 0;
}

/**
 * Handle a call to fseek() made by JavaScript.
 *
 * fseek expects 3 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: The offset to seek to
 *   2: An integer representing the whence parameter of standard fseek.
 *      whence = 0: seek from the beginning of the file
 *      whence = 1: seek from the current file position
 *      whence = 2: seek from the end of the file
 * on success, fseek returns a result in |output| separated by \1:
 *   0: "fseek"
 *   1: the file index
 *   2: The new file position
 * on failure, fseek returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleFseek(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  long offset;
  int whence;
  int result;

  if (num_params != 3) {
    *output = PrintfToNewString("Error: fseek takes 3 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  offset = strtol(params[1], NULL, 10);
  whence = strtol(params[2], NULL, 10);

  if (!file) {
    *output =
        PrintfToNewString("Error: Unknown file handle %s.", file_index_string);
    return 2;
  }

  result = fseek(file, offset, whence);
  if (result) {
    *output = PrintfToNewString("Error: fseek returned error %d.", result);
    return 3;
  }

  offset = ftell(file);
  if (offset < 0) {
    *output = PrintfToNewString(
        "Error: fseek succeeded, but ftell returned error %d.", offset);
    return 4;
  }

  *output = PrintfToNewString("fseek\1%s\1%d", file_index_string, offset);
  return 0;
}

/**
 * Handle a call to fclose() made by JavaScript.
 *
 * fclose expects 1 parameter:
 *   0: The index of the file (which is mapped to a FILE*)
 * on success, fclose returns a result in |output| separated by \1:
 *   0: "fclose"
 *   1: the file index
 * on failure, fclose returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleFclose(int num_params, char** params, char** output) {
  FILE* file;
  int file_index;
  const char* file_index_string;
  int result;

  if (num_params != 1) {
    *output = PrintfToNewString("Error: fclose takes 1 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, &file_index);
  if (!file) {
    *output =
        PrintfToNewString("Error: Unknown file handle %s.", file_index_string);
    return 2;
  }

  result = fclose(file);
  if (result) {
    *output = PrintfToNewString("Error: fclose returned error %d.", result);
    return 3;
  }

  RemoveFileFromMap(file_index);

  *output = PrintfToNewString("fclose\1%s", file_index_string);
  return 0;
}

/**
 * Handle a call to stat() made by JavaScript.
 *
 * stat expects 1 parameter:
 *   0: The name of the file
 * on success, stat returns a result in |output| separated by \1:
 *   0: "stat"
 *   1: the file name
 *   2: the size of the file
 * on failure, stat returns an error string in |output|.
 *
 * @param[in] num_params The number of params in |params|.
 * @param[in] params An array of strings, parameters to this function.
 * @param[out] output A string to write informational function output to.
 * @return An errorcode; 0 means success, anything else is a failure. */
int HandleStat(int num_params, char** params, char** output) {
  FILE* file;
  int file_index;
  const char* filename;
  const char* mode;

  if (num_params != 1) {
    *output = PrintfToNewString("Error: stat takes 1 parameter.");
    return 1;
  }

  filename = params[0];

  struct stat buf;
  memset(&buf, 0, sizeof(buf));
  int result = stat(filename, &buf);
  if (result == -1) {
    *output = PrintfToNewString("Error: stat returned error %d.", errno);
    return 2;
  }

  *output = PrintfToNewString("stat\1%s\1%d", filename, buf.st_size);
  return 0;
}
