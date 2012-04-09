/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_UTILS_MACROS_H_
#define LIBRARIES_UTILS_MACROS_H_

/**
* A macro to disallow the evil copy constructor and operator= functions
* This should be used in the private: declarations for a class.
*/
#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)


/**
* Macros to prevent name mangling of defnitions, allowing them to be
* referenced from C.
*/
#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
#else
# define EXTERN_C_BEGIN
# define EXEERN_C_END
#endif  /* __cplusplus */


#endif  /* LIBRARIES_UTILS_MACROS_H_ */
