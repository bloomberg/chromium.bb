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

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.TreeMap;

/**
 * Test ReportStorage and its implementation by simulating two pocesses
 * as producer and consuer.
 * 
 * To use this test with TestReportStorageConsumer:
 * > java TestReportStorageProducer /tmp/testdir
 * 
 * In another console,
 * > java TestReportStorageConsumer /tmp/testdir
 * 
 * Then watch output on both console.
 */

public class TestReportStorageProducer {

  /**
   * @param args
   */
  public static void main(String[] args) {
    String testdir = args[0];
    ReportStorage rs = null;
    try {
      rs = new ReportStorageLocalFileImpl(testdir);
    } catch (IOException e) {
      e.printStackTrace();
      System.exit(1);
    }
    
    produce(rs);
  }
  
  private static void produce(ReportStorage rs) {
    int i = 0;
    TreeMap<String, String> params = 
      new TreeMap<String, String>();
    String v = "hello";
    ByteArrayInputStream ba = new ByteArrayInputStream("hello world!".getBytes());
    while (true) {
      ba.reset();
      params.put(Integer.toString(i), v);
      String id = rs.getUniqueId(params);
      rs.saveAttributes(id, params);
      if (id == null) {
        System.exit(1);
      }
      try {
        rs.writeStreamToReport(id, ba, 0);
      } catch (FileNotFoundException e) {
        e.printStackTrace();
        break;
      } catch (IOException e) {
        e.printStackTrace();
        break;
      }
      i++;
      try {
        Thread.sleep(1000);
      } catch (InterruptedException e) {
        e.printStackTrace();
        break;
      }
    }
  }
}
