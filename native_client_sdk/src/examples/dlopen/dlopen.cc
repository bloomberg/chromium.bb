// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// @file
/// This example demonstrates building a dynamic library which is loaded by the
/// NaCl module.  To load the NaCl module, the browser first looks for the
/// CreateModule() factory method (at the end of this file).  It calls
/// CreateModule() once to load the module code from your .nexe.  After the
/// .nexe code is loaded, CreateModule() is not called again.
///
/// Once the .nexe code is loaded, the browser then calls the CreateInstance()
/// method on the object returned by CreateModule().  If the CreateInstance
/// returns successfully, then Init function is called, which will load the
/// shared object on a worker thread.  We use a worker because dlopen is
/// a blocking call, which is not allowed on the main thread.

#include <dlfcn.h> 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ppapi/cpp/completion_callback.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>

#include "eightball.h"
#include "nacl_io/nacl_io.h"
#include "reverse.h"


class DlopenInstance : public pp::Instance {
 public:
  explicit DlopenInstance(PP_Instance instance)
      : pp::Instance(instance),
        eightball_so_(NULL),
        reverse_so_(NULL),
        eightball_(NULL),
        reverse_(NULL),
        tid_(NULL) {}

  virtual ~DlopenInstance(){};

  // Helper function to post a message back to the JS and stdout functions.
  void logmsg(const char* pStr){
    PostMessage(pp::Var(std::string("log:") + pStr));
    fprintf(stdout, pStr);
  }

  // Initialize the module, staring a worker thread to load the shared object.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]){
    nacl_io_init_ppapi(pp_instance(),
                       pp::Module::Get()->get_browser_interface());
    // Mount a HTTP mount at /http. All reads from /http/* will read from the
    // server.
    mount("", "/http", "httpfs", 0, "");

    logmsg("Spawning thread to cache .so files...");
    if (pthread_create(&tid_, NULL, LoadLibrariesOnWorker, this)) {
      logmsg("ERROR; pthread_create() failed.\n");
      return false;
    }
    return true;
  }

  // This function is called on a worker thread, and will call dlopen to load
  // the shared object.  In addition, note that this function does NOT call
  // dlclose, which would close the shared object and unload it from memory.
  void LoadLibrary() {
    const int32_t IMMEDIATELY = 0;
    eightball_so_ = dlopen("libeightball.so", RTLD_LAZY);
    reverse_so_ = dlopen("/http/glibc/Debug/libreverse_x86_64.so", RTLD_LAZY);
    pp::CompletionCallback cc(LoadDoneCB, this);
    pp::Module::Get()->core()->CallOnMainThread(IMMEDIATELY, cc , 0);
  }

  // This function will run on the main thread and use the handle it stored by
  // the worker thread, assuming it successfully loaded, to get a pointer to the
  // message function in the shared object.
  void UseLibrary() {
    if (eightball_so_ != NULL) {
      intptr_t offset = (intptr_t) dlsym(eightball_so_, "Magic8Ball");
      eightball_ = (TYPE_eightball) offset;
      if (NULL == eightball_) {
          std::string message = "dlsym() returned NULL: ";
          message += dlerror();
          message += "\n";
          logmsg(message.c_str());
          return;
      }

      logmsg("Loaded libeightball.so");
    } else {
      logmsg("libeightball.so did not load");
    }


    if (reverse_so_ != NULL) {
      intptr_t offset = (intptr_t) dlsym(reverse_so_, "Reverse");
      reverse_ = (TYPE_reverse) offset;
      if (NULL == reverse_) {
          std::string message = "dlsym() returned NULL: ";
          message += dlerror();
          message += "\n";
          logmsg(message.c_str());
          return;
      }
      logmsg("Loaded libreverse.so");
    } else {
      logmsg("libreverse.so did not load");
    }
  }

  // Called by the browser to handle the postMessage() call in Javascript.
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string()) {
      logmsg("Message is not a string.");
      return;
    }

    std::string message = var_message.AsString();
    if (message == "eightball") {
      if (NULL == eightball_){
        logmsg("Eightball library not loaded");
        return;
      }

      std::string ballmessage = "The Magic 8-Ball says: ";
      ballmessage += eightball_();
      ballmessage += "!";

      logmsg(ballmessage.c_str());
    } else if (message.find("reverse:") == 0) {
      if (NULL == reverse_) {
        logmsg("Reverse library not loaded");
        return;
      }

      std::string s = message.substr(strlen("reverse:"));
      char* result = reverse_(s.c_str());

      std::string message = "Your string reversed: \"";
      message += result;
      message += "\"";

      free(result);

      logmsg(message.c_str());
    } else {
      std::string errormsg = "Unexpected message: ";
      errormsg += message + "\n";
      logmsg(errormsg.c_str());
    }
  }

  static void* LoadLibrariesOnWorker(void *pInst) {
    DlopenInstance *inst = static_cast<DlopenInstance *>(pInst);
    inst->LoadLibrary();
    return NULL;
  }

  static void LoadDoneCB(void *pInst, int32_t result) {
    DlopenInstance *inst = static_cast<DlopenInstance *>(pInst);
    inst->UseLibrary();
  }

 private:
  void* eightball_so_;
  void* reverse_so_;
  TYPE_eightball eightball_;
  TYPE_reverse reverse_;
  pthread_t tid_;
 };

// The Module class.  The browser calls the CreateInstance() method to create
// an instance of your NaCl module on the web page.  The browser creates a new
// instance for each <embed> tag with type="application/x-nacl".
class dlOpenModule : public pp::Module {
   public:
  dlOpenModule() : pp::Module() {}
  virtual ~dlOpenModule() {}

  // Create and return a DlopenInstance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new DlopenInstance(instance);
  }
};


// Factory function called by the browser when the module is first loaded.
// The browser keeps a singleton of this module.  It calls the
// CreateInstance() method on the object you return to make instances.  There
// is one instance per <embed> tag on the page.  This is the main binding
// point for your NaCl module with the browser.
namespace pp {
  Module* CreateModule() {
    return new dlOpenModule();
  }
}  // namespace pp

