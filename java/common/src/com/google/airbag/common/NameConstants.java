/* Copyright (C) 2006 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

package com.google.airbag.common;

/** A class defines URL parameter names. */

public class NameConstants {
  // URL parameter names
  // product name
  public static final String PRODUCT_PNAME = "prod";
  // version 
  public static final String VERSION_PNAME = "ver";
  // application or module
  public static final String APPLICATION_PNAME = "app";
  // platform, e.g., win32, linux, mac
  public static final String PLATFORM_PNAME = "plat";
  // report format, e.g., dump, xml
  public static final String FORMAT_PNAME = "fmt";
  // process uptime
  public static final String PROCESSUPTIME_PNAME = "procup";
  // cumulative process uptime
  public static final String CUMULATIVEUPTIME_PNAME = "cumup";
  // time when report is created
  public static final String REPORTTIME_PNAME = "rept";
  // a random number
  public static final String RANDOMNUM_PNAME  = "rand";
  // report checksum
  public static final String CHECKSUM_PNAME = "sum";
}
