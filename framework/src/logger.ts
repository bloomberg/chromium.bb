import { getStackTrace } from "./util.js";

export abstract class Logger {
  protected namespacePath: string[] = [];

  public push(namespace: string) {
    if (namespace.indexOf("\n") !== -1) {
      throw new Error("namespace should not have newlines");
    }
    this.namespacePath.push(namespace);
  }

  public pop() {
    this.namespacePath.pop();
  }

  public path() {
    return this.namespacePath.slice();
  }

  public expect(cond: boolean, msg?: string) {
    if (cond) {
      this.log("PASS");
    } else {
      if (msg) {
        this.log("FAIL: " + msg);
      } else {
        this.log("FAIL");
      }
      this.log(getStackTrace());
    }
  }

  public abstract log(s: string): void;
}

function applyIndent(indent: string, s: string) {
  const lines = s.split("\n");
  return indent + lines.join("\n" + indent);
}

abstract class TextLogger extends Logger {
  public push(namespace: string) {
    this.out(applyIndent(this.mkIndent(), "* " + namespace));
    super.push(namespace);
  }

  public log(s: string) {
    this.out(applyIndent(this.mkIndent() + "| ", s));
  }

  protected abstract out(s: string): void;

  private mkIndent(): string {
    return "  ".repeat(this.namespacePath.length);
  }
}

export class StringLogger extends TextLogger {
  private contents = "";

  public getContents(): string {
    return this.contents;
  }

  protected out(s: string) {
    this.contents += (this.contents ? "\n" : "") + s;
  }
}

export class ConsoleLogger extends TextLogger {
  protected out(s: string) {
    // tslint:disable-next-line:no-console
    console.log(s);
  }
}
