/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef HANDLERS_H_
#define HANDLERS_H_

#define MAX_PARAMS 4

typedef int (*HandleFunc)(int num_params, char** params, char** output);

int HandleFopen(int num_params, char** params, char** output);
int HandleFwrite(int num_params, char** params, char** output);
int HandleFread(int num_params, char** params, char** output);
int HandleFseek(int num_params, char** params, char** output);
int HandleFclose(int num_params, char** params, char** output);
int HandleStat(int num_params, char** params, char** output);

int HandleOpendir(int num_params, char** params, char** output);
int HandleReaddir(int num_params, char** params, char** output);
int HandleClosedir(int num_params, char** params, char** output);

int HandleMkdir(int num_params, char** params, char** output);
int HandleRmdir(int num_params, char** params, char** output);
int HandleChdir(int num_params, char** params, char** output);
int HandleGetcwd(int num_params, char** params, char** output);

int HandleGethostbyname(int num_params, char** params, char** output);
int HandleConnect(int num_params, char** params, char** output);
int HandleSend(int num_params, char** params, char** output);
int HandleRecv(int num_params, char** params, char** output);
int HandleClose(int num_params, char** params, char** output);

#endif /* HANDLERS_H_ */
