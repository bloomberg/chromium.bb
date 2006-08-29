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

import java.util.SortedMap;
import java.io.InputStream;

/**
 * A common interface for different multipart HttpServletRequest 
 * implementations. The interface is simple enough to be used by the
 * upload server.
 */

public interface MultipartRequest {
  /**
   * Returns a sorted map of name to values of an HTTP request.
   */
  public SortedMap<String, String> getParameters();

  /**
   * Returns an input stream of uploading file. 
   */
  public InputStream getInputStream();
}
