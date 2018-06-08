// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ASM5;
import static org.objectweb.asm.Opcodes.ASTORE;
import static org.objectweb.asm.Opcodes.ATHROW;
import static org.objectweb.asm.Opcodes.GOTO;
import static org.objectweb.asm.Opcodes.ILOAD;
import static org.objectweb.asm.Opcodes.INVOKESTATIC;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.IRETURN;
import static org.objectweb.asm.Opcodes.ISTORE;
import static org.objectweb.asm.Opcodes.RETURN;

import static org.chromium.bytecode.TypeUtils.STRING;
import static org.chromium.bytecode.TypeUtils.VOID;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.util.Arrays;

/**
 * An ClassVisitor for adding TraceEvent.begin and TraceEvent.end methods to any method with a
 * &#64;{@link org.chromium.base.annotations.TraceEvent} annotation.
 */
class TraceEventAnnotationClassAdapter extends ClassVisitor {
    private static final String ANNOTATION_STRING = "@org.chromium.base.annotations.TraceEvent";
    private static final String ANNOTATION_DESCRIPTOR =
            "Lorg/chromium/base/annotations/TraceEvent;";
    private static final String TRACE_EVENT_DESCRIPTOR = "org/chromium/base/TraceEvent";
    private static final String TRACE_EVENT_SIGNATURE = TypeUtils.getMethodDescriptor(VOID, STRING);
    private static final String THROWABLE_DESCRIPTOR = "java/lang/Throwable";
    private static final String MOVED_METHOD_PREFIX = "wrappedByTraceEvent$";

    private String mClassName;
    private ClassUtils mClassUtils;
    private Class<?> mClass;

    TraceEventAnnotationClassAdapter(
            ClassVisitor visitor, String className, ClassLoader classLoader) {
        super(ASM5, visitor);
        mClassName = className;
        mClassUtils = new ClassUtils(classLoader);
    }

    @Override
    public void visit(int version, int access, String name, String signature, String superName,
            String[] interfaces) {
        super.visit(version, access, name, signature, superName, interfaces);
        mClass = mClassUtils.loadClass(name);
        mClassName = name;
    }

    @Override
    public MethodVisitor visitMethod(final int access, final String name, String desc,
            String signature, String[] exceptions) {
        MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);

        boolean hasAnnotation = checkAnnotation(name, desc);
        if (!hasAnnotation) {
            return mv;
        } else {
            writeTryFinallyHelper(mv, access, name, desc);

            // Duplicate the method
            return super.visitMethod(
                    access, MOVED_METHOD_PREFIX + name, desc, signature, exceptions);
        }
    }

    /**
     * Writes the necessary bytecode for calling TraceEvent methods wrapped in a try-catch block.
     *
     * @param mv a newly created method visitor
     * @param access bit flag representing the access of the method (i.e.: ACC_STATIC, ACC_PUBLIC).
     * @param name name of the method (i.e.: "foo")
     * @param descriptor signature of the method (i.e.: "(Ljava/lang/String;)V")
     */
    private void writeTryFinallyHelper(
            MethodVisitor mv, final int access, final String name, String descriptor) {
        Type[] argTypes = Type.getArgumentTypes(descriptor);
        Type returnType = Type.getReturnType(descriptor);
        boolean isStatic = (access & ACC_STATIC) == ACC_STATIC;

        String eventName = mClassName.substring(mClassName.lastIndexOf('/') + 1) + "." + name;

        mv.visitCode();
        Label start = new Label();
        Label end = new Label();
        Label handler = new Label();
        mv.visitTryCatchBlock(start, end, handler, THROWABLE_DESCRIPTOR);

        // TraceEvent.begin(String)
        mv.visitLdcInsn(eventName);
        mv.visitMethodInsn(
                INVOKESTATIC, TRACE_EVENT_DESCRIPTOR, "begin", TRACE_EVENT_SIGNATURE, false);
        // try {
        mv.visitLabel(start);
        // var valueToReturn = <name>(<desc>)
        int i = 0;
        if (!isStatic) {
            // 0 now is used for `this`
            mv.visitVarInsn(ALOAD, 0);
            i = 1;
        }

        for (Type arg : argTypes) {
            mv.visitVarInsn(arg.getOpcode(ILOAD), i);

            if (arg.equals(Type.LONG_TYPE)) {
                i += 2;
            } else {
                i++;
            }
        }
        int returnIndex = i + 1;

        String innerName = MOVED_METHOD_PREFIX + name;
        if (isStatic) {
            mv.visitMethodInsn(INVOKESTATIC, mClassName, innerName, descriptor, false);
        } else {
            mv.visitMethodInsn(INVOKEVIRTUAL, mClassName, innerName, descriptor, false);
        }
        if (!returnType.equals(Type.VOID_TYPE)) {
            mv.visitVarInsn(returnType.getOpcode(ISTORE), returnIndex);
        }
        // TraceEvent.end(String)
        mv.visitLdcInsn(eventName);
        mv.visitMethodInsn(
                INVOKESTATIC, TRACE_EVENT_DESCRIPTOR, "end", TRACE_EVENT_SIGNATURE, false);
        // return valueToReturn
        if (!returnType.equals(Type.VOID_TYPE)) {
            mv.visitVarInsn(returnType.getOpcode(ILOAD), returnIndex);
        }
        // }
        mv.visitLabel(end);
        Label ambientReturn = null;
        if (!returnType.equals(Type.VOID_TYPE)) {
            mv.visitInsn(returnType.getOpcode(IRETURN));
        } else {
            ambientReturn = new Label();
            mv.visitJumpInsn(GOTO, ambientReturn);
        }
        // catch (Throwable e) {
        mv.visitLabel(handler);
        mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {THROWABLE_DESCRIPTOR});
        mv.visitVarInsn(ASTORE, returnIndex);
        // TraceEvent.end(String)
        mv.visitLdcInsn(eventName);
        mv.visitMethodInsn(
                INVOKESTATIC, TRACE_EVENT_DESCRIPTOR, "end", TRACE_EVENT_SIGNATURE, false);
        // throw e;
        mv.visitVarInsn(ALOAD, returnIndex);
        mv.visitInsn(ATHROW);
        // }
        if (returnType.equals(Type.VOID_TYPE)) {
            mv.visitLabel(ambientReturn);
            mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
            mv.visitInsn(RETURN);
        }
        int maxStack = Math.max(1, argTypes.length);
        int maxLocals = returnIndex + 1;
        mv.visitMaxs(maxStack, maxLocals);
        mv.visitEnd();
    }

    /**
     * Checks if the {@link org.chromium.base.annotations.TraceEvent} annotation is present on the
     * method with the given name and descriptor inside the currently visited class.
     *
     * @param name name of the method, ie: "foo"
     * @param descriptor describes the method signature, ie: "()V"
     * @return true if the {@link org.chromium.base.annotations.TraceEvent} annotation is present.
     */
    private boolean checkAnnotation(String name, String descriptor) {
        Class<?>[] parameterTypes = getMethodParameterTypes(descriptor);

        AnnotatedElement method;
        try {
            switch (name) {
                case "<clinit>":
                    return false;
                case "<init>":
                    method = mClass.getDeclaredConstructor(parameterTypes);
                    break;
                default:
                    method = mClass.getDeclaredMethod(name, parameterTypes);
                    break;
            }
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        }

        Annotation[] annotations = method.getAnnotations();
        return Arrays.stream(annotations).anyMatch(a -> a.toString().startsWith(ANNOTATION_STRING));
    }

    /**
     * Converts the ASM method descriptor into an array of {@link Class} objects.
     *
     * @param descriptor method signature descriptor
     * @return array of Class objects, where position in the array corresponds to the location
     * of the argument for the method.
     */
    private Class<?>[] getMethodParameterTypes(String descriptor) {
        Type[] argTypes = Type.getArgumentTypes(descriptor);
        return Arrays.stream(argTypes).map(mClassUtils::convert).toArray(Class[] ::new);
    }
}
