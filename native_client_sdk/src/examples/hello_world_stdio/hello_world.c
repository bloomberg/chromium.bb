#include <stdio.h>
#include <string.h>


#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_console.h"

#include "ppapi_main/ppapi_main.h"


// Have the Module object provided by ppapi_main create a basic
// PPAPI instance with default arguments to setting up
// stdio file handles, processing keyboard, etc...
// PPAPI Instance with default arguments.
PPAPI_MAIN_WITH_DEFAULT_ARGS

static PPB_Messaging* ppb_messaging_interface = NULL;
static PPB_Var* ppb_var_interface = NULL;
static PPB_Console* ppb_console_interface = NULL;


/**
 * Creates new string PP_Var from C string. The resulting object will be a
 * refcounted string object. It will be AddRef()ed for the caller. When the
 * caller is done with it, it should be Release()d.
 * @param[in] str C string to be converted to PP_Var
 * @return PP_Var containing string.
 */
static struct PP_Var CStrToVar(const char* str) {
  if (ppb_var_interface != NULL) {
    return ppb_var_interface->VarFromUtf8(str, strlen(str));
  }
  return PP_MakeUndefined();
}

//
// Post a message back to JavaScript
//
static void SendMessage(const char *str) {
  PP_Instance instance = PPAPI_GetInstanceId();
  if (ppb_messaging_interface)
    ppb_messaging_interface->PostMessage(instance, CStrToVar(str));
}

//
// Send a message to the JavaScript Console
//
static void LogMessage(const char *str) {
  PP_Instance instance = PPAPI_GetInstanceId();
  if (ppb_console_interface)
    ppb_console_interface->Log(instance, PP_LOGLEVEL_LOG,
                          CStrToVar(str));
}

//
// The "main" entry point called by PPAPIInstance once initialization
// takes place.  This is called off the main thread, which is hidden
// from the developer, making it safe to use blocking calls.
// The arguments are provided as:
//   argv[0] = "NEXE"
//   argv[1] = "--<KEY>"
//   argv[2] = "<VALUE>"
// Where the embed tag for this module uses KEY=VALUE
//
int ppapi_main(int argc, const char *argv[]) {
  ppb_messaging_interface =
      (PPB_Messaging*) PPAPI_GetInterface(PPB_MESSAGING_INTERFACE);
  ppb_var_interface =
      (PPB_Var*) PPAPI_GetInterface(PPB_VAR_INTERFACE);
  ppb_console_interface =
      (PPB_Console*) PPAPI_GetInterface(PPB_CONSOLE_INTERFACE);

  SendMessage("Hello World STDIO.\n");
  LogMessage("Hello World STDERR.\n");

  return 0;
}
