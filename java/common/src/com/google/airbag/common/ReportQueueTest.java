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

import java.io.IOException;
import java.util.LinkedList;

/**
 * A test class for ReportQueue implementations, currently tests
 * ReportQueueDirImpl and ReportQueueFileImpl.
 */
public class ReportQueueTest {

  public static void main(String[] args) {
    ReportQueue rq = new ReportQueueFileImpl("/tmp/rqtest");
    runTest(rq);
    
    try {
      rq = new ReportQueueDirImpl("/tmp/rqdir");
    } catch (IOException e) {
      e.printStackTrace();
      System.exit(1);
    }
    runTest(rq);
    
    System.out.println("OK");
  }
  
  private static void runTest(ReportQueue rq) {
    rq.enqueue("hello");
    rq.enqueue("world");
    
    LinkedList<String> v = rq.dequeue();
    assert v.size() == 2;
    
    assert v.get(0).equals("hello");
    assert v.get(1).equals("world");
    assert rq.empty();
    
    v.remove();
    
    rq.enqueue(v);
    assert !rq.empty();
    
    v = rq.dequeue();
    assert v.size() == 1;
    assert v.get(0).equals("world");
  }
}
