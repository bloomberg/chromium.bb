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

import java.util.LinkedList;

/**
 * A queue interface for unprocessed report ids. The interface is intended
 * for inter-process usage. A report uploading server enqueues new report
 * ids, and a processor dequeues ids. 
 * 
 * The interface is much simpler than <b>java.util.Queue</b>. An implementation
 * should provide a persistent storage of queued ids even when a process 
 * is killed.
 */

public interface ReportQueue {
  /**
   * Enqueue a record id.
   * 
   * @param rid
   * @return true if success
   */
  public boolean enqueue(String rid);
  
  /**
   * Enqueue a list of ids
   * 
   * @param ids
   * @return true if success
   */
  public boolean enqueue(LinkedList<String> ids);
  
  /**
   * Checks if a queue is empty
   * @return true if the queue is empty
   */
  public boolean empty();
  
  /**
   * Removes several ids from the queue. An implementation decides how 
   * many ids to be removed.
   * 
   * @return a list of queue
   */
  public LinkedList<String> dequeue();
}
